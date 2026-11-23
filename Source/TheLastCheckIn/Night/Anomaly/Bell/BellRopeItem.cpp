#include "Night/Anomaly/Bell/BellRopeItem.h"
#include "TheLastCheckInCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

ABellRopeItem::ABellRopeItem()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	PickupPrompt = NSLOCTEXT("Anomaly", "BellRopePickup", "줄 들기");
	DropPrompt = NSLOCTEXT("Anomaly", "BellRopeDrop", "줄 내려놓기");
}

ABellRopeItem* ABellRopeItem::FindHeldBy(const AActor* Interactor)
{
	UWorld* World = Interactor ? Interactor->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ABellRopeItem> It(World); It; ++It)
	{
		if (It->Holder.Get() == Interactor)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ABellRopeItem::CanInteract_Implementation(AActor* Interactor) const
{
	return !Holder.IsValid() || Holder.Get() == Interactor;
}

FText ABellRopeItem::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return Holder.Get() == Interactor ? DropPrompt : PickupPrompt;
}

void ABellRopeItem::Interact_Implementation(AActor* Interactor)
{
	if (Holder.Get() == Interactor)
	{
		Unequip();
	}
	else if (!Holder.IsValid())
	{
		Equip(Interactor);
	}
}

/*
들고 있는 동안은 다음 E가 줄로 가서 내려놓기가 됨
*/
bool ABellRopeItem::IsInteractionActive_Implementation(AActor* Interactor) const
{
	return Holder.Get() == Interactor;
}

void ABellRopeItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Holder.Reset();
	Super::EndPlay(EndPlayReason);
}

/*
1인칭 팔 소켓이 있으면 손에 쥐고, 없으면 카메라 앞에 고정
*/
void ABellRopeItem::Equip(AActor* Interactor)
{
	UCameraComponent* Camera = Interactor ? Interactor->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return;
	}
	Holder = Interactor;
	WorldScale = GetActorScale3D();
	Mesh->SetSimulatePhysics(false);
	SetActorEnableCollision(false);

	const ATheLastCheckInCharacter* Character = Cast<ATheLastCheckInCharacter>(Interactor);
	USkeletalMeshComponent* Arms = Character ? Character->GetFirstPersonMesh() : nullptr;
	const bool bInHand = Arms && !HandSocket.IsNone() && Arms->DoesSocketExist(HandSocket);
	if (bInHand)
	{
		AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
	}
	else
	{
		AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	const FTransform Hold = bInHand ? HandTransform : HeldOffset;
	SetActorRelativeLocation(Hold.GetLocation());
	SetActorRelativeRotation(Hold.GetRotation());
	SetActorScale3D(WorldScale);

	// 팔과 같은 1인칭 FOV/스케일로 그려 벽에 파묻히지 않게 함
	SetFirstPersonRender(true);
}

/*
시선이 닿는 곳(DropDistance 안)에 내려놓음 - 벽을 보고 있으면 벽 앞, 허공이면 시선 끝 지점 아래 바닥
*/
void ABellRopeItem::Unequip()
{
	AActor* OldHolder = Holder.Get();
	Holder.Reset();
	SetFirstPersonRender(false);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorScale3D(WorldScale);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BellRopeDrop), false, this);
	if (OldHolder)
	{
		Params.AddIgnoredActor(OldHolder);
	}

	/*
	1. 시선 방향으로 트레이스해서 놓을 지점을 찾음
	*/
	FVector ViewLocation = GetActorLocation();
	FRotator ViewRotation = GetActorRotation();
	if (const APawn* Pawn = Cast<APawn>(OldHolder))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}
	const FVector ViewEnd = ViewLocation + ViewRotation.Vector() * DropDistance;
	FHitResult ViewHit;
	FVector DropPoint = ViewEnd;
	if (GetWorld()->LineTraceSingleByChannel(ViewHit, ViewLocation, ViewEnd, ECC_Visibility, Params))
	{
		// 닿은 면에서 살짝 띄워 벽이나 물건 속에 묻히지 않게 함
		DropPoint = ViewHit.ImpactPoint + ViewHit.ImpactNormal * 5.0f;
	}

	/*
	2. 그 지점 아래 바닥을 찾아 모델 바닥 중앙이 닿도록 놓음
	*/
	SetActorRotation(FRotator(0.0f, ViewRotation.Yaw, 0.0f));
	SetActorLocation(DropPoint);
	const FBox Bounds = Mesh->Bounds.GetBox();
	const FVector BottomCenter(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
	const FVector PivotFromBottom = GetActorLocation() - BottomCenter;
	const FVector Start(DropPoint.X, DropPoint.Y, DropPoint.Z + 5.0f);
	FHitResult FloorHit;
	if (GetWorld()->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0.0f, 0.0f, 1000.0f), ECC_Visibility, Params))
	{
		SetActorLocation(FloorHit.ImpactPoint + PivotFromBottom);
	}
	SetActorEnableCollision(true);
}

void ABellRopeItem::SetFirstPersonRender(bool bFirstPerson)
{
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* Prim : Prims)
	{
		Prim->FirstPersonPrimitiveType = bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None;
		Prim->MarkRenderStateDirty();
	}
}
