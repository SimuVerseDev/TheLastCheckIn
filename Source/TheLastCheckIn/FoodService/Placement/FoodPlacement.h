#pragma once

#include "CoreMinimal.h"

class AActor;
class AFoodItemActor;
class UFoodSnapSlotComponent;

namespace FoodPlacement
{
	THELASTCHECKIN_API AFoodItemActor* GetHeldFood(AActor* Interactor);

	THELASTCHECKIN_API bool TryPlaceHeldFood(AActor* Interactor, UFoodSnapSlotComponent* Slot);
}