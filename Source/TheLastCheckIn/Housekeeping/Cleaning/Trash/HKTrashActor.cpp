#include "Housekeeping/Cleaning/Trash/HKTrashActor.h"
#include "Housekeeping/Cleaning/Trash/HKTrashDirtComponent.h"
#include "Housekeeping/Cleaning/Trash/System/HKTrashPoolSubsystem.h"
#include "Components/StaticMeshComponent.h"

AHKTrashActor::AHKTrashActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 흡수 연출 중에만 Tick

	Dirt = CreateDefaultSubobject<UHKTrashDirtComponent>(TEXT("TrashDirt"));
	Mesh->SetRelativeScale3D(FVector::OneVector); // 부모의 테스트 큐브용 0.3 대신 원본 크기

	PickupPrompt = NSLOCTEXT("Housekeeping", "TrashPickupPrompt", "줍기");
	AbsorbPrompt = NSLOCTEXT("Housekeeping", "TrashAbsorbPrompt", "카트에 넣기");
	bCanBePickedUp = false; // 쓰레기는 들지 않고 흡수만
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
	if (State != EHKTrashState::Active || !Target)
	{
		return false;
	}

	State = EHKTrashState::Absorbing;
	AbsorbTarget = Target;
	AbsorbStartLocation = GetActorLocation();
	AbsorbElapsed = 0.0f;

	SetHighlighted(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(true);
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