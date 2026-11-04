#include "Housekeeping/Objects/Stain/HKWipeToolComponent.h"
#include "UObject/UObjectIterator.h"

UHKWipeToolComponent* UHKWipeToolComponent::FindHeldBy(const AActor* Interactor)
{
	if (!Interactor)
	{
		return nullptr;
	}
	for (TObjectIterator<UHKWipeToolComponent> It; It; ++It)
	{
		if (It->GetWorld() == Interactor->GetWorld() && It->GetUser() == Interactor)
		{
			return *It;
		}
	}
	return nullptr;
}
