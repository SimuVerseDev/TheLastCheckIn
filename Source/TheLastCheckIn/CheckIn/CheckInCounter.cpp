#include "CheckInCounter.h"
#include "CheckInInteractionComponent.h"

bool ACheckInCounter::CanInteract_Implementation(AActor* Interactor) const
{
	const UCheckInInteractionComponent* CheckIn = Interactor ? Interactor->FindComponentByClass<UCheckInInteractionComponent>() : nullptr;
	return CheckIn && CheckIn->CanOpenCheckIn();
}

FText ACheckInCounter::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("CheckIn", "CounterInteractPrompt", "체크인");
}

void ACheckInCounter::Interact_Implementation(AActor* Interactor)
{
	if (UCheckInInteractionComponent* CheckIn = Interactor ? Interactor->FindComponentByClass<UCheckInInteractionComponent>() : nullptr)
	{
		CheckIn->OpenCheckIn();
	}
}
