#include "Housekeeping/Carry/PickupActor/HKPhysicsPickupActor.h"
#include "Housekeeping/Carry/HKCarryComponent.h"
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

void AHKPhysicsPickupActor::NotifyPickedUp(AActor* InCarrier)
{
	Carrier = InCarrier;
	BP_OnPickedUp(InCarrier);
}

void AHKPhysicsPickupActor::NotifyReleased(bool bThrown)
{
	Carrier.Reset();
	BP_OnReleased(bThrown);
}

bool AHKPhysicsPickupActor::CanBePickedUp() const
{
	return bCanBePickedUp && !IsCarried() && Mesh->IsSimulatingPhysics();
}

/*
HKInteractable
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
	return Carry && CanBePickedUp() && Carry->TryPickup(Mesh);
}
