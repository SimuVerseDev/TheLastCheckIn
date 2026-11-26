#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FoodSnapSlotComponent.generated.h"

class AFoodItemActor;

UENUM(BlueprintType)
enum class EFoodSlotRule : uint8
{
	Any,
	RawOnly,
	ReadyToEat
};

UCLASS(ClassGroup = (Food), meta = (BlueprintSpawnableComponent))
class THELASTCHECKIN_API UFoodSnapSlotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food|Slot")
	EFoodSlotRule Rule = EFoodSlotRule::ReadyToEat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food|Slot")
	bool bAllowPickup = true;

	UFUNCTION(BlueprintPure, Category = "Food|Slot")
	AFoodItemActor* GetFood() const;

	UFUNCTION(BlueprintPure, Category = "Food|Slot")
	bool CanAccept(const AFoodItemActor* Food) const;

	UFUNCTION(BlueprintCallable, Category = "Food|Slot")
	bool TryPlaceFood(AFoodItemActor* Food);

	// 부착과 물리는 건드리지 않고 양쪽 참조만 해제
	void ForgetFood(AFoodItemActor* Food);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AFoodItemActor> Occupant;

	FGuid OccupantLease;
};