#include "Housekeeping/Core/Interaction/HKInteractable.h"

bool IHKInteractable::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

FText IHKInteractable::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("Housekeeping", "DefaultInteractPrompt", "상호작용");
}

float IHKInteractable::GetInteractHoldDuration_Implementation(AActor* Interactor) const
{
	return 0.0f;
}

void IHKInteractable::Interact_Implementation(AActor* Interactor)
{
}

bool IHKInteractable::CanUse_Implementation(AActor* Interactor) const
{
	return false;
}

FText IHKInteractable::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("Housekeeping", "DefaultUsePrompt", "사용");
}

bool IHKInteractable::Use_Implementation(AActor* Interactor)
{
	return false;
}

void IHKInteractable::OnInteractFocusBegin_Implementation(AActor* Interactor)
{
}

void IHKInteractable::OnInteractFocusEnd_Implementation(AActor* Interactor)
{
}