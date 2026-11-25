#include "Housekeeping/Objects/Trash/HKTrashActor.h"
#include "Housekeeping/Objects/Trash/HKTrashDirtComponent.h"
#include "Housekeeping/Objects/Trash/HKTrashPoolSubsystem.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Housekeeping/Objects/Trash/HKTrashBinComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

AHKTrashActor::AHKTrashActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Dirt = CreateDefaultSubobject<UHKTrashDirtComponent>(TEXT("TrashDirt"));
	Mesh->SetRelativeScale3D(FVector::OneVector);

	PickupPrompt = NSLOCTEXT("Housekeeping", "TrashPickupPrompt", "줍기");
	AbsorbPrompt = NSLOCTEXT("Housekeeping", "TrashAbsorbPrompt", "카트에 넣기");
	bCanBePickedUp = false;
}

/*
Public
*/
void AHKTrashActor::BeginPlay()
{
	Super::BeginPlay();
	OriginalScale = GetActorScale3D();
}

void AHKTrashActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State != EHKTrashState::Absorbing)
	{
		SetActorTickEnabled(false);
		return;
	}

	AbsorbElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(AbsorbElapsed / AbsorbDuration, 0.0f, 1.0f);
	const float Ease = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 3.0f);
	
	const USceneComponent* Target = AbsorbTarget.Get();
	const FVector End = Target ? Target->GetComponentLocation() : AbsorbStartLocation;

	SetActorLocation(FMath::Lerp(AbsorbStartLocation, End, Ease));
	SetActorScale3D(OriginalScale * (1.0f - Ease * 0.9f));

	if (Alpha >= 1.0f)
	{
		if (DropInSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DropInSound, End);
		}
		BP_OnAbsorbed();
		/*
		null인데 returnPool호출 시 크래시 방지용
		*/
		if (UHKTrashPoolSubsystem* Pool = GetWorld()->GetSubsystem<UHKTrashPoolSubsystem>())
		{
			Pool->ReturnToPool(this);
		}
		else
		{
			EnterPool();
		}
	}
}

bool AHKTrashActor::Absorb(USceneComponent* Target)
{
	if (State != EHKTrashState::Active || !IsValid(Target))
	{
		return false;
	}

	AActor* TargetOwner = Target->GetOwner();

	UHKTrashBinComponent* Bin = IsValid(TargetOwner)
		? TargetOwner->FindComponentByClass<UHKTrashBinComponent>()
		: nullptr;

	// 실제 흡수 시점에도 남은 용량을 확인합니다.
	if (!IsValid(Bin) || !Bin->CanAcceptTrash(1))
	{
		return false;
	}

	// 용량 변경 알림이 발생하기 전에 상태를 바꿔
	// 같은 쓰레기가 중복으로 흡수되지 않게 합니다.
	State = EHKTrashState::Absorbing;
	AbsorbTarget = Target;
	AbsorbStartLocation = GetActorLocation();
	AbsorbElapsed = 0.0f;

	// 흡수 중인 쓰레기도 용량을 차지합니다.
	if (!Bin->TryAddTrash(1))
	{
		State = EHKTrashState::Active;
		AbsorbTarget.Reset();
		return false;
	}

	SetHighlighted(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(true);

	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, AbsorbStartLocation);
	}
	return true;
}

/*
메시의 형태를 변경
*/
void AHKTrashActor::EnterPool()
{
	State = EHKTrashState::Pooled;
	AbsorbTarget.Reset();
	SetActorTickEnabled(false);
	SetHighlighted(false);

	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	Dirt->ClearDirt();
}

void AHKTrashActor::ExitPool(const FTransform& SpawnTransform)
{
	State = EHKTrashState::Active;

	SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorScale3D(OriginalScale);
	SetActorHiddenInGame(false);

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

void AHKTrashActor::SetHighlighted(bool bHighlighted)
{
	Mesh->SetOverlayMaterial(bHighlighted ? HighlightMaterial.Get() : nullptr);
}

/*
IInteractable
*/
bool AHKTrashActor::CanUse_Implementation(AActor* Interactor) const
{
	const UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	return State == EHKTrashState::Active && Carry && Carry->CanAbsorbTrash();
}

FText AHKTrashActor::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return AbsorbPrompt;
}

bool AHKTrashActor::Use_Implementation(AActor* Interactor)
{
	const UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	return Carry && Absorb(Carry->GetTrashAbsorbTarget());
}

void AHKTrashActor::OnInteractFocusBegin_Implementation(AActor* Interactor)
{
	SetHighlighted(true);
}

void AHKTrashActor::OnInteractFocusEnd_Implementation(AActor* Interactor)
{
	SetHighlighted(false);
}