#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FoodService/Data/DiningTypes.h"
#include "GuestDiningComponent.generated.h"

class ABuffetStation;
class ADiningSeat;
class AGuestCharacter;
class ATargetPoint;
class UFoodDefinition;

// 손님 한 명의 식당 이동, 착석, 주문, 객실 복귀를 관리
UCLASS(ClassGroup = (Guests), meta = (BlueprintSpawnableComponent))
class THELASTCHECKIN_API UGuestDiningComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGuestDiningComponent();

    // 방문 준비와 출발.
    bool TryStartBreakfast(
        const TArray<TObjectPtr<ABuffetStation>>& Stations,
        const TArray<TObjectPtr<ADiningSeat>>& Seats,
        double CurrentSeconds);

    // 이 손님이 출발하기 전에 좌석과 메뉴를 확보한다.
    bool PrepareDinner(
        const TArray<TObjectPtr<ADiningSeat>>& Seats,
        const TArray<TObjectPtr<UFoodDefinition>>& Menu,
        double CurrentSeconds);

    bool StartPreparedDinner();

    // 객실에서 대기 중이며 식사나 이상행동에 참여하지 않는지 확인한다.
    bool IsAvailableForDining(double CurrentSeconds) const
    {
        return GetGuestReadyToVisit(CurrentSeconds) != nullptr;
    }

    // 현재 투숙 중 오늘 석식에 이미 출발했는지 확인한다.
    bool HasVisitedDinner(double CurrentSeconds) const;

    // 출발 전이면 예약을 해제하고, 출발 후이면 객실로 복귀한다.
    void CancelDinnerDeparture();

    void AdvanceDining(double CurrentSeconds, bool bBreakfastOpen);

    UFUNCTION(BlueprintPure, Category = "Guests|Dining")
    bool IsBusy() const
    {
        return bVisitActive;
    }

    // 주문 조회와 접수.
    UFUNCTION(BlueprintPure, Category = "Guests|Dining")
    bool CanTakeDinnerOrder() const;

    UFUNCTION(BlueprintCallable, Category = "Guests|Dining")
    bool TakeDinnerOrder();

    UFUNCTION(BlueprintPure, Category = "Guests|Dining")
    bool GetDinnerOrder(FFoodOrderRecord& OutOrder) const;

    // 주문에 기록된 투숙 번호와 현재 석식 방문 번호를 확인한다.
    bool MatchesDinnerVisit(const FGuid& StayId, const FGuid& InVisitId) const
    {
        return bVisitActive
            && MealType == EGuestMealType::Dinner
            && VisitStayId == StayId
            && DiningVisitId == InVisitId;
    }

    // 외부 이동 요청과 풀 반환.
    bool PrepareForMovement();
    void ResetForPool();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 음식 지급 기능을 연결하기 전, 뷔페 앞에서 기다릴 게임 시간.
    UPROPERTY(EditAnywhere, Category = "Dining|Preview",
        meta = (ClampMin = "0.1", Units = "min"))
    float BuffetWaitGameMinutes = 5.0f;

    // 조식 착석 후 머무를 게임 시간.
    UPROPERTY(EditAnywhere, Category = "Dining|Seat",
        meta = (ClampMin = "0.1", Units = "min"))
    float SeatStayGameMinutes = 30.0f;

    // 음식 제공 기능을 연결하기 전, 석식 착석 후 머무를 게임 시간.
    UPROPERTY(EditAnywhere, Category = "Dining|Dinner",
        meta = (ClampMin = "0.1", Units = "min"))
    float DinnerStayGameMinutes = 30.0f;

    // 이동이 끝나지 않으면 복귀를 시도할 월드 시간.
    UPROPERTY(EditAnywhere, Category = "Dining|Movement",
        meta = (ClampMin = "1.0", Units = "s"))
    float MovementTimeoutSeconds = 60.0f;

private:
    // 조식과 석식에 공통으로 적용할 출발 조건과 방문 기록.
    AGuestCharacter* GetGuestReadyToVisit(double CurrentSeconds) const;
    void BeginVisit(EGuestMealType InMealType, const FGuid& StayId, int64 DayIndex);

    // 조식 줄서기.
    void UpdateQueue(double WorldNow);

    // 좌석 예약, 이동, 착석, 퇴장.
    bool TryReserveSeat(
        const TArray<TObjectPtr<ADiningSeat>>& Seats);

    void BeginMoveToSeat();
    bool TryMoveToSeat(AGuestCharacter* Guest, ATargetPoint* ApproachPoint);
    bool TrySitDown(double CurrentSeconds);
    bool TryLeaveSeat();

    bool CanPlaceGuest(const FVector& CapsuleCenter, const AActor* IgnoredActor) const;

    void RestoreSeatMovement();
    void ReleaseSeatReservation();

    // 석식 주문 생성과 취소.
    bool CreateDinnerOrder(double CurrentSeconds);
    void CancelDinnerOrder(EFoodOrderCancelReason Reason);

    // 객실 복귀와 방문 정리.
    void BeginReturnToRoom();
    void ReleaseBuffetReservation();
    void ClearVisit();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Dining",
        meta = (AllowPrivateAccess = "true"))
    EGuestMealType MealType = EGuestMealType::None;

    UPROPERTY(Transient)
    TWeakObjectPtr<ABuffetStation> Buffet;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> QueueTarget;

    UPROPERTY(Transient)
    TWeakObjectPtr<ADiningSeat> Seat;

    UPROPERTY(Transient)
    TObjectPtr<UFoodDefinition> PendingDinnerFood;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Dining|Order",
        meta = (AllowPrivateAccess = "true"))
    FGuid DinnerOrderId;

    // 뷔페 이용 예약 번호.
    FGuid VisitId;

    // 좌석 예약 번호.
    FGuid SeatReservationId;

    // 식당 방문 전체를 구분하는 번호.
    FGuid DiningVisitId;

    FGuid VisitStayId;
    int64 VisitDay = -1;

    // 조식과 석식의 당일 방문 기록은 각각 관리한다.
    FGuid LastBreakfastStayId;
    int64 LastBreakfastDay = -1;

    FGuid LastDinnerStayId;
    int64 LastDinnerDay = -1;

    double WaitUntilSeconds = -1.0;
    double MoveDeadlineWorldSeconds = -1.0;
    double NextReturnAttemptWorldSeconds = -1.0;
    double NextStartAttemptWorldSeconds = 0.0;

    FVector SeatExitFloorLocation = FVector::ZeroVector;

    uint8 SavedMovementMode = 0;
    uint8 SavedCustomMovementMode = 0;

    bool bVisitActive = false;
    bool bReturning = false;
    bool bPlacedAtSeat = false;

    // 좌석과 메뉴를 확보하고 출발을 기다리는 상태.
    bool bDinnerPrepared = false;
};
