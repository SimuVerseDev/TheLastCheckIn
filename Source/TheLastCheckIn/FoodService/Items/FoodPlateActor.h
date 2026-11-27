#pragma once

#include "CoreMinimal.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "FoodPlateActor.generated.h"

class AFoodItemActor;
class UFoodSnapSlotComponent;

UCLASS()
class THELASTCHECKIN_API AFoodPlateActor : public AHKPhysicsPickupActor
{
	GENERATED_BODY()

public:
	AFoodPlateActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Food")
	UFoodSnapSlotComponent* GetFoodSlot() const
	{
		return FoodSlot.Get();
	}

	UFUNCTION(BlueprintPure, Category = "Food")
	AFoodItemActor* GetFood() const;

	UFUNCTION(BlueprintPure, Category = "Food")
	int32 GetFoodCount() const;

	void SetGuestOwned();
	void ClearFood();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool CanInteractWhileCarrying_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food")
	TObjectPtr<UFoodSnapSlotComponent> FoodSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food")
	TArray<TObjectPtr<UFoodSnapSlotComponent>> FoodSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Layout", meta = (ClampMin = "0.0"))
	FVector2D FoodSlotSpacing = FVector2D(6.0, 8.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Layout")
	float FoodSlotHeight = 3.0f;

private:
	void UpdateSlotLayout();
	UFoodSnapSlotComponent* FindEmptySlot(const AFoodItemActor* Food) const;

	bool bGuestOwned = false;
};
