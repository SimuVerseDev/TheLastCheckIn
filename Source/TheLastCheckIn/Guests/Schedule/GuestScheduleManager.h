#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "GuestScheduleManager.generated.h"

class UGameClockSubsystem;
class AGuestPoolManager;
class AGuestCharacter;
class ARoomActor;
class UGuestParty;
class ABuffetStation;
class ADiningSeat;
class UFoodDefinition;

struct FGuestDinnerPlan
{
    double ScheduledSeconds = 0.0;
    double NextAttemptWorldSeconds = 0.0;

    // 일행 전원이 석식에 출발했으면 당일 출발 시도를 마친다.
    bool bStarted = false;
};

UCLASS()
class THELASTCHECKIN_API AGuestScheduleManager : public AActor
{
    GENERATED_BODY()

public:
    AGuestScheduleManager();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AGuestPoolManager> GuestPool;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AActor> SpawnPoint;

    // 두 번째~네 번째 손님이 등장할 위치(일행)
    // 첫 번째 손님은 SpawnPoint 사용
    UPROPERTY(EditInstanceOnly, Category = "Schedule|Party")
    TArray<TObjectPtr<AActor>> CompanionSpawnPoints;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AActor> CounterTarget;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AActor> LobbyTarget;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AActor> ExitTarget;

    // 이 호텔에서 사용할 객실 목록
    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TArray<TObjectPtr<ARoomActor>> Rooms;

    UPROPERTY(EditAnywhere, Category = "Schedule|Guests")
    FDataTableRowHandle GuestTypeRow;

    UPROPERTY(EditAnywhere, Category = "Schedule|Guests",
        meta = (ClampMin = "0"))
    int32 DailyGuestLimit = 12;

    // 한 일행의 최소 인원
    UPROPERTY(EditAnywhere, Category = "Schedule|Party",
        meta = (ClampMin = "1", ClampMax = "4"))
    int32 MinPartySize = 1;

    // 한 일행의 최대 인원
    UPROPERTY(EditAnywhere, Category = "Schedule|Party",
        meta = (ClampMin = "1", ClampMax = "4"))
    int32 MaxPartySize = 4;

    // 생성 실패 후 재시도 횟수. 풀 부족 대기는 횟수에 포함하지 않음.
    UPROPERTY(EditAnywhere, Category = "Schedule|Recovery", meta = (ClampMin = "0"))
    int32 MaxSpawnRetries = 3;

    // 게임 내부 시간 기준
    UPROPERTY(EditAnywhere, Category = "Schedule|Recovery", meta = (ClampMin = "1"))
    int32 SpawnRetryIntervalMinutes = 1;

    // 호텔 시계 배속과 무관한 월드 시간. 일시정지 중에는 흐르지 않음.
    UPROPERTY(EditAnywhere, Category = "Schedule|Recovery", meta = (ClampMin = "1.0"))
    float MovementTimeoutSeconds = 60.0f;

    UPROPERTY(EditAnywhere, Category = "Schedule|Recovery", meta = (ClampMin = "0"))
    int32 MaxMovementRetries = 3;

    // 게임 내부 시간 기준 최소 등장 간격
    UPROPERTY(EditAnywhere, Category = "Schedule|Guests",
    meta = (ClampMin = "1"))
    int32 MinArrivalIntervalMinutes = 10;

    UPROPERTY(EditAnywhere, Category = "Schedule|Guests",
        meta = (ClampMin = "1"))
    int32 MaxArrivalIntervalMinutes = 30;

    UPROPERTY(EditAnywhere, Category = "Schedule|Breakfast")
    bool bEnableBreakfast = true;

    UPROPERTY(EditAnywhere, Category = "Schedule|Breakfast",
        meta = (ClampMin = "0", ClampMax = "23"))
    int32 BreakfastStartHour = 6;

    UPROPERTY(EditAnywhere, Category = "Schedule|Breakfast",
        meta = (ClampMin = "1", ClampMax = "24"))
    int32 BreakfastEndHour = 8;

    // 이 매니저가 손님을 보낼 조식 진열대
    UPROPERTY(EditInstanceOnly, Category = "Schedule|Breakfast")
    TArray<TObjectPtr<ABuffetStation>> BreakfastBuffets;

    // 조식과 석식 손님이 함께 사용할 좌석.
    UPROPERTY(EditInstanceOnly, Category = "Schedule|Dining")
    TArray<TObjectPtr<ADiningSeat>> DiningSeats;

    UPROPERTY(EditAnywhere, Category = "Schedule|Dinner")
    bool bEnableDinner = true;

    UPROPERTY(EditAnywhere, Category = "Schedule|Dinner",
        meta = (ClampMin = "0", ClampMax = "23"))
    int32 DinnerStartHour = 16;

    UPROPERTY(EditAnywhere, Category = "Schedule|Dinner",
        meta = (ClampMin = "1", ClampMax = "24"))
    int32 DinnerEndHour = 20;

    // 석식 출발 후에도 객실에 남겨 둘 대기 인원.
    UPROPERTY(EditAnywhere, Category = "Schedule|Dinner",
        meta = (ClampMin = "1"))
    int32 MinRoomGuestsForAnomaly = 1;

    // 현재는 스테이크와 샐러드 데이터 에셋을 등록한다.
    UPROPERTY(EditAnywhere, Category = "Schedule|Dinner")
    TArray<TObjectPtr<UFoodDefinition>> DinnerMenu;

private:
    UFUNCTION()
    void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

    // 입실과 객실 배정.
    void TryAdmitGuest(double CurrentSeconds);
    bool HasAvailableRoom() const;

    UFUNCTION()
    void HandlePartyRoomAssigned(UGuestParty* Party);

    UFUNCTION()
    void HandlePartyCheckedIn(UGuestParty* Party);

    // 식당 방문 진행과 석식 교대.
    void UpdateDining(double CurrentSeconds);
    void UpdateDinner(double CurrentSeconds);

    bool IsDinnerCandidate(const UGuestParty* Party, double CurrentSeconds) const;
    int32 CountAvailableRoomGuests(double CurrentSeconds) const;

    // 일부라도 출발했다면 true를 반환
    bool TryStartPartyDinner(UGuestParty* Party, double CurrentSeconds);

    TWeakObjectPtr<UGameClockSubsystem> Clock;

    int32 NextPartySize = 0;
    int32 SpawnFailureCount = 0;
    bool bArrivalsDisabled = false;
    int64 ArrivalDayIndex = -1;
    int32 AdmittedToday = 0;
    double NextArrivalSeconds = 0.0;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGuestParty>> ActiveParties;

    // 투숙 번호별 당일 석식 일정.
    TMap<FGuid, FGuestDinnerPlan> DinnerPlans;
    int64 DinnerPlanDay = -1;
};
