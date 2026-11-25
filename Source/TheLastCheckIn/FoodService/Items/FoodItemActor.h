#pragma once

#include "CoreMinimal.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "FoodService/Data/FoodDefinition.h"
#include "FoodItemActor.generated.h"

class UFoodPoolSubsystem;
class ABuffetStation;

// 집기와 운반이 가능한 음식 액터. 사용 후 풀 반환.
UCLASS()
class THELASTCHECKIN_API AFoodItemActor : public AHKPhysicsPickupActor
{
    GENERATED_BODY()

    friend class UFoodPoolSubsystem;
    friend class ABuffetStation;

public:
    AFoodItemActor();

    UFUNCTION(BlueprintPure, Category = "Food")
    UFoodDefinition* GetFoodDefinition() const
    {
        return Definition.Get();
    }

    UFUNCTION(BlueprintPure, Category = "Food")
    int32 GetServings() const
    {
        return Servings;
    }

    UFUNCTION(BlueprintPure, Category = "Food")
    EFoodPresentation GetPresentation() const
    {
        return Presentation;
    }

    UFUNCTION(BlueprintPure, Category = "Food|Pool")
    bool IsActiveFood() const
    {
        return bActiveFood;
    }

    // 현재 대여 번호. 풀에서 꺼낸 직후 저장해 두고 풀 반환 시 전달한다.
    UFUNCTION(BlueprintPure, Category = "Food|Pool")
    FGuid GetLeaseId() const
    {
        return LeaseId;
    }

    // 사용 중인 음식의 대여 번호가 일치하는지 확인한다.
    UFUNCTION(BlueprintPure, Category = "Food|Pool")
    bool MatchesLease(FGuid ExpectedLeaseId) const;

private:
    // 음식 종류, 인분 수, 위치를 설정하고 액터를 활성화한다.
    void ActivateFood(
        UFoodDefinition* InDefinition,
        int32 InServings,
        EFoodPresentation InPresentation,
        const FTransform& Transform);

    // 음식 정보와 움직임을 초기화하고 액터를 숨긴다.
    void ResetForPool();

    UPROPERTY(Transient)
    TObjectPtr<UFoodDefinition> Definition;

    UPROPERTY(Transient)
    TWeakObjectPtr<UFoodPoolSubsystem> PoolOwner;

    // 이 액터에 남아 있는 인분 수
    int32 Servings = 0;

    EFoodPresentation Presentation = EFoodPresentation::Batch;

    // 재사용 전후를 구분하는 대여 번호
    FGuid LeaseId;

    bool bActiveFood = false;
};
