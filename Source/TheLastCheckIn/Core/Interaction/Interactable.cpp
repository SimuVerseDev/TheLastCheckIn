#include "Core/Interaction/Interactable.h"

bool IInteractable::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

FText IInteractable::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("Housekeeping", "DefaultInteractPrompt", "상호작용");
}

float IInteractable::GetInteractHoldDuration_Implementation(AActor* Interactor) const
{
	return 0.0f;
}

void IInteractable::Interact_Implementation(AActor* Interactor)
{
}

bool IInteractable::CanUse_Implementation(AActor* Interactor) const
{
	return false;
}

FText IInteractable::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("Housekeeping", "DefaultUsePrompt", "사용");
}

bool IInteractable::Use_Implementation(AActor* Interactor)
{
	return false;
}

void IInteractable::OnInteractFocusBegin_Implementation(AActor* Interactor)
{
}

void IInteractable::OnInteractFocusEnd_Implementation(AActor* Interactor)
{
}