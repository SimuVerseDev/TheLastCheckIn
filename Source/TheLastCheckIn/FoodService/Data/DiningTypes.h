#pragma once

#include "CoreMinimal.h"
#include "DiningTypes.generated.h"

class AGuestCharacter;
class UFoodDefinition;

UENUM(BlueprintType)
enum class EGuestMealType : uint8
{
    None,
    Breakfast,
    Dinner
};

UENUM(BlueprintType)
enum class EFoodOrderService : uint8
{
    Restaurant,
    RoomService
};

UENUM(BlueprintType)
enum class EFoodOrderState : uint8
{
    PendingAcceptance, // 플레이어가 주문을 받기 전.
    Accepted,          // 주문 접수 완료.
    Cancelled          // 손님 복귀 등으로 주문 취소.
};

UENUM(BlueprintType)
enum class EFoodOrderCancelReason : uint8
{
    None,
    VisitEnded,        // 식당 방문 종료.
    PreviewTimeEnded   // 음식 제공 기능 연결 전, 착석 대기 시간이 끝나 주문 취소.
};

USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FFoodOrderLine
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    TObjectPtr<UFoodDefinition> Food = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FFoodOrderRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    FGuid OrderId;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    TWeakObjectPtr<AGuestCharacter> Guest;

    // 손님 액터가 재사용되어도 이전 주문과 구분한다.
    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    FGuid StayId;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    FGuid VisitId;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    EFoodOrderService Service = EFoodOrderService::Restaurant;

    // 나중에 한 주문에 여러 음식을 담을 수 있도록 배열로 관리한다.
    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    TArray<FFoodOrderLine> Items;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    EFoodOrderState State = EFoodOrderState::PendingAcceptance;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    EFoodOrderCancelReason CancelReason = EFoodOrderCancelReason::None;

    UPROPERTY(BlueprintReadOnly, Category = "Food Order")
    double CreatedAtSeconds = 0.0;
};
