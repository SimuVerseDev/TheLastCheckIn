#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuffetStation.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UFoodDefinition;
class AFoodItemActor;
class UArrowComponent;
class ATargetPoint;

// 음식 한 종류의 재고, 대기열, 이용 자리와 1인분 제공을 관리한다.
UCLASS()
class THELASTCHECKIN_API ABuffetStation : public AActor
{
    GENERATED_BODY()

public:
    ABuffetStation();

    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    int32 TryAddFood(
        AFoodItemActor* Food,
        FGuid ExpectedLeaseId);

    UFUNCTION(BlueprintPure, Category = "Food|Buffet")
    bool HasFood() const;

    UFUNCTION(BlueprintPure, Category = "Food|Buffet")
    int32 GetRemainingServings() const
    {
        return RemainingServings;
    }

    UFUNCTION(BlueprintPure, Category = "Food|Buffet")
    UFoodDefinition* GetFoodDefinition() const
    {
        return StockDefinition.Get();
    }

    // 줄에 등록한다. 대기 위치가 모두 배정되었으면 실패한다.
    bool TryJoinQueue(AActor* Requester, FGuid RequestId);

    // 현재 순서에 해당하는 대기 위치를 반환한다.
    AActor* GetQueuePoint(AActor* Requester, FGuid RequestId);

    // 줄에서 제거하고 해당 방문의 음식 자리 예약도 해제한다.
    void LeaveQueue(AActor* Requester, FGuid RequestId);

    // 줄의 맨 앞 손님만 예약할 수 있다. RequestId는 방문 번호다.
    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    bool TryReserveServingPoint(
        AActor* Requester,
        FGuid RequestId);

    // 해당 손님의 방문 번호와 일치하는 예약을 해제한다.
    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    void ReleaseServingPoint(
        AActor* Requester,
        FGuid RequestId);

    // 예약한 손님에게 1인분 접시를 제공하고 재고를 줄인다. 실패 시 nullptr.
    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    AFoodItemActor* TryTakeServing(
        AActor* Requester,
        FGuid RequestId,
        const FTransform& PlateTransform);

    // Arrow에 설정된 손님의 도착 후 방향
    FRotator GetServingRotation() const;

protected:
    // 맨 앞부터 뒤쪽 순서로 지정한다. 각 위치는 한 명만 사용한다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Food|Queue")
    TArray<TObjectPtr<ATargetPoint>> QueuePoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    // 테이블이나 진열대 자체의 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> StationMesh;

    // 진열된 음식 표시. 보충한 음식의 BatchMesh를 사용한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> FoodDisplay;

    // 손님이 설 수 있는 영역. 진열대 앞 NavMesh 위에 배치한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> ArrivalArea;

    // 진열할 수 있는 최대 인분 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Buffet",
        meta = (ClampMin = "1"))
    int32 Capacity = 30;

    // 손님이 바라볼 방향. 화살표가 음식 쪽을 향하도록 배치한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UArrowComponent> ServingDirection;

private:
    struct FQueueEntry
    {
        TWeakObjectPtr<AActor> Guest;
        FGuid VisitId;
    };

    TArray<FQueueEntry> WaitingQueue;

    void RemoveInvalidQueueGuests();
    void RefreshFoodDisplay();

    bool OwnsReservation(
        AActor* Requester,
        FGuid RequestId) const;

    UPROPERTY(Transient)
    TObjectPtr<UFoodDefinition> StockDefinition;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> ReservedGuest;

    // 현재 예약한 손님의 방문 번호
    FGuid ReservationId;

    // 진열대에 남아 있는 인분 수
    int32 RemainingServings = 0;

    // 보충·수령 처리 중 같은 함수가 다시 실행되는 것을 막는다.
    bool bUpdatingStock = false;
};
