#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"
#include "Housekeeping/Core/Dirt/HKPlacementDirtComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "UObject/ConstructorHelpers.h"

AHKPhysicsPickupActor::AHKPhysicsPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetGenerateOverlapEvents(true);

	// 테스트용 기본 메시(BP에서 교체)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.3f));
	}
	PickupPrompt = NSLOCTEXT("Housekeeping", "PickupPrompt", "잡기");
}

void AHKPhysicsPickupActor::BeginPlay()
{
	Super::BeginPlay();

	if (bRestUntilPickedUp)
	{
		Mesh->SetSimulatePhysics(false);
	}
}

void AHKPhysicsPickupActor::NotifyPickedUp(AActor* InCarrier)
{
	Carrier = InCarrier;
	BP_OnPickedUp(InCarrier);
}

void AHKPhysicsPickupActor::NotifyReleased(bool bThrown)
{
	Carrier.Reset();
	/*
	던진 게 아니라 내려놓은 경우만 제자리 스냅
	*/
	if (!bThrown)
	{
		if (UHKPlacementDirtComponent* Placement = FindComponentByClass<UHKPlacementDirtComponent>())
		{
			Placement->TrySnapHome();
		}
	}
	BP_OnReleased(bThrown);
}

bool AHKPhysicsPickupActor::CanBePickedUp() const
{
	const UHKPlacementDirtComponent* Placement = FindComponentByClass<UHKPlacementDirtComponent>();
	const bool bResting = Placement && !Placement->IsSnapping();
	return bCanBePickedUp && !IsCarried() && (Mesh->IsSimulatingPhysics() || bResting || bRestUntilPickedUp);
}

/*
Interactable
*/
bool AHKPhysicsPickupActor::CanInteract_Implementation(AActor* Interactor) const
{
	return false;
}

bool AHKPhysicsPickupActor::CanUse_Implementation(AActor* Interactor) const
{
	const UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	return Carry && !Carry->IsCarrying() && CanBePickedUp();
}

FText AHKPhysicsPickupActor::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return PickupPrompt;
}

bool AHKPhysicsPickupActor::Use_Implementation(AActor* Interactor)
{
	UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	if (!Carry || !CanBePickedUp())
	{
		return false;
	}
	const bool bWasSimulating = Mesh->IsSimulatingPhysics();
	Mesh->SetSimulatePhysics(true);
	if (Carry->TryPickup(Mesh))
	{
		return true;
	}
	Mesh->SetSimulatePhysics(bWasSimulating);
	return false;
}
