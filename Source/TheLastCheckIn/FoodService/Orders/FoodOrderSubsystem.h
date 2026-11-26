#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FoodService/Data/DiningTypes.h"
#include "FoodOrderSubsystem.generated.h"

class AGuestCharacter;
class UFoodDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFoodOrdersChanged);

UCLASS(BlueprintType)
class THELASTCHECKIN_API UFoodOrderSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 주문 생성, 접수, 취소.
    FGuid CreateRestaurantOrder(
        AGuestCharacter* Guest,
        const FGuid& StayId,
        const FGuid& VisitId,
        UFoodDefinition* Food,
        double CurrentSeconds);

    bool TryAcceptOrder(
        const FGuid& OrderId,
        AGuestCharacter* Guest,
        const FGuid& StayId,
        const FGuid& VisitId);

    void CancelOrder(const FGuid& OrderId, EFoodOrderCancelReason Reason);

    // 주문 조회와 UI 갱신.
    UFUNCTION(BlueprintPure, Category = "Food Order")
    bool GetOrder(const FGuid& OrderId, FFoodOrderRecord& OutOrder) const;

    // 현재 방문에 속한 접수 대기, 접수 완료 주문을 반환
    UFUNCTION(BlueprintPure, Category = "Food Order")
    TArray<FFoodOrderRecord> GetOpenOrders() const;

    UPROPERTY(BlueprintAssignable, Category = "Food Order")
    FFoodOrdersChanged OnOrdersChanged;

    virtual void Deinitialize() override;

private:
    bool IsCurrentCustomer(const FFoodOrderRecord& Order) const;

    UPROPERTY(Transient)
    TMap<FGuid, FFoodOrderRecord> Orders;
};
