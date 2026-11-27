#pragma once

#include "CoreMinimal.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "FoodService/Data/FoodDefinition.h"
#include "FoodItemActor.generated.h"

class UFoodPoolSubsystem;
class UFoodSnapSlotComponent;

UCLASS()
class THELASTCHECKIN_API AFoodItemActor : public AHKPhysicsPickupActor
{
    GENERATED_BODY()

    friend class UFoodPoolSubsystem;
    friend class UFoodSnapSlotComponent;

public:
    AFoodItemActor();

    UFUNCTION(BlueprintPure, Category = "Food")
    UFoodDefinition* GetFoodDefinition() const
    {
        return Definition.Get();
    }

    UFUNCTION(BlueprintPure, Category = "Food")
    EFoodCookState GetCookState() const
    {
        return CookState;
    }

    UFUNCTION(BlueprintPure, Category = "Food")
    bool IsReadyToEat() const;

    UFUNCTION(BlueprintPure, Category = "Food")
    bool IsActiveFood() const
    {
        return bActiveFood;
    }

    UFUNCTION(BlueprintPure, Category = "Food|Pool")
    FGuid GetLeaseId() const
    {
        return LeaseId;
    }

    bool MatchesLease(FGuid ExpectedLeaseId) const;

    UFoodSnapSlotComponent* GetCurrentSlot() const;

    bool BeginCooking();
    void FinishCooking();
    void CancelCooking();

    virtual bool CanUse_Implementation(AActor* Interactor) const override;
    virtual bool Use_Implementation(AActor* Interactor) override;

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 레벨에 직접 배치한 음식에도 지정할 수 있다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
    TObjectPtr<UFoodDefinition> Definition;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
    EFoodCookState CookState = EFoodCookState::Raw;

private:
    void ActivateFood(
        UFoodDefinition* InDefinition,
        EFoodCookState InState,
        const FTransform& Transform);

    void ResetForPool();
    void RefreshAppearance();

    UPROPERTY(Transient)
    TWeakObjectPtr<UFoodSnapSlotComponent> CurrentSlot;

    UPROPERTY(Transient)
    TWeakObjectPtr<UFoodPoolSubsystem> PoolOwner;

    FGuid LeaseId;
    bool bActiveFood = false;
};
