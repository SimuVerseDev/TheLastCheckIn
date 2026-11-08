#include "Night/NightBedActor.h"
#include "Night/NightFlowComponent.h"
#include "Components/StaticMeshComponent.h"

ANightBedActor::ANightBedActor()
{
	PrimaryActorTick.bCanEverTick = false;
	BedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BedMesh"));
	SetRootComponent(BedMesh);
	BedMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

bool ANightBedActor::CanInteract_Implementation(AActor* Interactor) const
{
	const UNightFlowComponent* Flow = UNightFlowComponent::FindForActor(Interactor);
	return Flow && Flow->CanShowSleepPrompt();
}

FText ANightBedActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("Night", "Sleep", "잠자기");
}

float ANightBedActor::GetInteractHoldDuration_Implementation(AActor* Interactor) const
{
	return 0.0f;
}

void ANightBedActor::Interact_Implementation(AActor* Interactor)
{
	if (UNightFlowComponent* Flow = UNightFlowComponent::FindForActor(Interactor)) Flow->RequestSleep();
}

bool ANightBedActor::CanUse_Implementation(AActor* Interactor) const
{
	return false;
}
