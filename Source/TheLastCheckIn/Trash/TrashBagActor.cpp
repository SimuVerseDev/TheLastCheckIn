#include "Trash/TrashBagActor.h"
#include "Trash/TrashBagHolderComponent.h"
#include "Trash/TrashCanActor.h"
#include "TheLastCheckIn.h"
#include "TheLastCheckInCharacter.h"
#include "Core/Interaction/PlayerInteractionComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ATrashBagActor::ATrashBagActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PickupPrompt = NSLOCTEXT("Trash", "BagPickup", "쓰레기봉투 집기");
	DropPrompt = NSLOCTEXT("Trash", "BagDrop", "쓰레기봉투 내려놓기");
	DisposePrompt = NSLOCTEXT("Trash", "BagDispose", "쓰레기봉투 버리기");
}

bool ATrashBagActor::CanHolderTakeBag(AActor* Holder)
{
	const ATheLastCheckInCharacter* Player = Cast<ATheLastCheckInCharacter>(Holder);
	if (!IsValid(Player) || !IsValid(Player->GetFirstPersonMesh()))
	{
		return false;
	}
	if (const UTrashBagHolderComponent* BagHolder = Player->FindComponentByClass<UTrashBagHolderComponent>())
	{
		if (BagHolder->IsHolding())
		{
			return false;
		}
	}
	UPlayerInteractionComponent* Interaction = Player->FindComponentByClass<UPlayerInteractionComponent>();
	if (!IsValid(Interaction) || Interaction->HasActiveInteraction())
	{
		return false;
	}

	// 물건을 들고 있는 등 다른 작업 중이면 불가
	for (UActorComponent* Component : Player->GetComponents())
	{
		if (const IInteractionExtension* Extension = Cast<IInteractionExtension>(Component))
		{
			if (Extension->BlocksFocus())
			{
				return false;
			}
		}
	}
	return true;
}

bool ATrashBagActor::TryEquip(AActor* Holder)
{
	if (!CanHolderTakeBag(Holder) || GetOwner() != nullptr || !Mesh->GetStaticMesh())
	{
		return false;
	}

	ATheLastCheckInCharacter* Player = CastChecked<ATheLastCheckInCharacter>(Holder);
	USkeletalMeshComponent* HandMesh = Player->GetFirstPersonMesh();
	if (!AttachSocket.IsNone() && !HandMesh->DoesSocketExist(AttachSocket))
	{
		UE_LOG(LogTheLastCheckIn, Warning, TEXT("[TrashBag] 소켓을 찾을 수 없음: %s"), *AttachSocket.ToString());
		return false;
	}

	const bool bWasSimulating = Mesh->IsSimulatingPhysics();
	Mesh->SetSimulatePhysics(false);
	if (!AttachToComponent(HandMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket))
	{
		Mesh->SetSimulatePhysics(bWasSimulating);
		return false;
	}

	SetOwner(Player);
	SetActorRelativeTransform(HandTransform);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetOnlyOwnerSee(true);
	Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	if (UTrashBagHolderComponent* BagHolder = UTrashBagHolderComponent::FindOrAdd(Player))
	{
		BagHolder->SetHeldBag(this);
	}
	return true;
}

void ATrashBagActor::Drop(bool bEnablePhysics)
{
	if (!IsHeld())
	{
		return;
	}
	ReleaseFromHolder();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetOnlyOwnerSee(false);
	Mesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetSimulatePhysics(bEnablePhysics);
	if (bEnablePhysics)
	{
		Mesh->WakeAllRigidBodies();
	}
}

bool ATrashBagActor::IsHeldBy(const AActor* Holder) const
{
	return Holder && GetOwner() == Holder;
}

bool ATrashBagActor::IsHeld() const
{
	return GetOwner() != nullptr;
}

/*
IInteractable
*/
bool ATrashBagActor::CanInteract_Implementation(AActor* Interactor) const
{
	if (IsHeldBy(Interactor))
	{
		return true;
	}
	return !IsHeld() && CanHolderTakeBag(Interactor);
}

FText ATrashBagActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	if (IsHeldBy(Interactor))
	{
		return FindAimedTrashCan(Interactor) ? DisposePrompt : DropPrompt;
	}
	return PickupPrompt;
}

void ATrashBagActor::Interact_Implementation(AActor* Interactor)
{
	if (!IsHeldBy(Interactor))
	{
		TryEquip(Interactor);
		return;
	}

	if (ATrashCanActor* TrashCan = FindAimedTrashCan(Interactor))
	{
		if (TrashCan->TryDispose(this))
		{
			return;
		}
	}
	Drop();
}

bool ATrashBagActor::IsInteractionActive_Implementation(AActor* Interactor) const
{
	return IsHeldBy(Interactor);
}

/*
ITrashDisposable
*/
bool ATrashBagActor::CanDispose(const ATrashCanActor* TrashCan) const
{
	return IsHeld();
}

void ATrashBagActor::OnDisposed(ATrashCanActor* TrashCan)
{
	ReleaseFromHolder();
	OnBagDisposed.Broadcast(this);
	Destroy();
}

void ATrashBagActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseFromHolder();
	Super::EndPlay(EndPlayReason);
}

/*
private
*/
ATrashCanActor* ATrashBagActor::FindAimedTrashCan(AActor* Holder) const
{
	const APawn* Pawn = Cast<APawn>(Holder);
	const APlayerController* PC = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	if (!PC)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 3인칭이면 카메라가 폰 뒤에 있으니 그만큼 거리 증가 (PlayerInteractionComponent와 동일)
	const float CameraOffset = FVector::Dist(ViewLocation, Pawn->GetPawnViewLocation());
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * (DisposeRange + CameraOffset);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrashBagAim), false, Pawn);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		return nullptr;
	}
	return Cast<ATrashCanActor>(Hit.GetActor());
}

void ATrashBagActor::ReleaseFromHolder()
{
	if (AActor* Holder = GetOwner())
	{
		if (UTrashBagHolderComponent* BagHolder = Holder->FindComponentByClass<UTrashBagHolderComponent>())
		{
			BagHolder->ClearHeldBag(this);
		}
	}
	SetOwner(nullptr);
}
