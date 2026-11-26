#pragma once

#include "CoreMinimal.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "FoodPlateActor.generated.h"

class UFoodSnapSlotComponent;

UCLASS()
class THELASTCHECKIN_API AFoodPlateActor : public AHKPhysicsPickupActor
{
	GENERATED_BODY()

public:
	AFoodPlateActor();

	UFUNCTION(BlueprintPure, Category = "Food")
	UFoodSnapSlotComponent* GetFoodSlot() const
	{
		return FoodSlot.Get();
	}

	void SetGuestOwned();
	void ClearFood();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool CanInteractWhileCarrying_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food")
	TObjectPtr<UFoodSnapSlotComponent> FoodSlot;

private:
	bool bGuestOwned = false;
};
