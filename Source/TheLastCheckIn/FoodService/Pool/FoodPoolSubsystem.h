#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FoodService/Data/FoodDefinition.h"
#include "FoodPoolSubsystem.generated.h"

class AFoodItemActor;

// 음식 액터의 대여, 풀 반환, 재사용을 관리한다.
UCLASS()
class THELASTCHECKIN_API UFoodPoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 음식 액터를 풀에서 꺼낸다. 대기 중인 액터가 없으면 새로 생성한다.
    UFUNCTION(BlueprintCallable, Category = "Food|Pool")
    AFoodItemActor* AcquireFood(
        UFoodDefinition* Definition,
        EFoodCookState InitialState,
        const FTransform& Transform);

    // 음식을 풀에 반환한다. 대여 당시 저장한 LeaseId를 전달한다.
    // 대여 번호가 다르거나 들고 있는 음식은 반환할 수 없다.
    UFUNCTION(BlueprintCallable, Category = "Food|Pool")
    bool ReleaseFood(
        AFoodItemActor* Food,
        FGuid ExpectedLeaseId);

    // 대기 액터를 목표 개수까지 미리 생성한다. 최대 MaxRetainedFoods개.
    UFUNCTION(BlueprintCallable, Category = "Food|Pool")
    void WarmupPool(int32 DesiredCount = 16);

    // 풀에서 바로 꺼내 쓸 수 있는 대기 액터 수
    UFUNCTION(BlueprintPure, Category = "Food|Pool")
    int32 GetAvailableCount() const;

    virtual void Deinitialize() override;

private:
    AFoodItemActor* CreateFoodActor();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AFoodItemActor>> AvailableFoods;

    // 풀에 보관할 최대 개수. 이를 초과해 반환된 액터는 제거한다.
    static constexpr int32 MaxRetainedFoods = 64;

    bool bShuttingDown = false;
};
