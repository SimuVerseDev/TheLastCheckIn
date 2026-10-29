#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "GuestScheduleManager.generated.h"

class UGameClockSubsystem;
class AGuestPoolManager;
class AGuestCharacter;
class ARoomActor;

UCLASS()
class THELASTCHECKIN_API AGuestScheduleManager : public AActor
{
    GENERATED_BODY()

public:
    AGuestScheduleManager();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AGuestPoolManager> GuestPool;

    UPROPERTY(EditInstanceOnly, Category = "Schedule|References")
    TObjectPtr<AActor> SpawnPoint;

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
    int32 DailyGuestLimit = 3;

    // 게임 내부 시간 기준 최소 등장 간격
    UPROPERTY(EditAnywhere, Category = "Schedule|Guests",
    meta = (ClampMin = "1"))
    int32 MinArrivalIntervalMinutes = 10;

    UPROPERTY(EditAnywhere, Category = "Schedule|Guests",
        meta = (ClampMin = "1"))
    int32 MaxArrivalIntervalMinutes = 30;

    UPROPERTY(EditAnywhere, Category = "Schedule|Hours",
        meta = (ClampMin = "0", ClampMax = "23"))
    int32 CheckInStartHour = 14;

    UPROPERTY(EditAnywhere, Category = "Schedule|Hours",
        meta = (ClampMin = "1", ClampMax = "24"))
    int32 CheckInEndHour = 18;

    UPROPERTY(EditAnywhere, Category = "Schedule|Hours",
        meta = (ClampMin = "0", ClampMax = "23"))
    int32 CheckoutStartHour = 6;

    UPROPERTY(EditAnywhere, Category = "Schedule|Hours",
        meta = (ClampMin = "0", ClampMax = "23"))
    int32 CheckoutEndHour = 11;

private:
    struct FCheckoutSchedule
    {
        TWeakObjectPtr<AGuestCharacter> Guest;

        FGuid StayId;

        // 날짜까지 포함한 누적 게임 초
        double CheckoutAtSeconds = 0.0;

        // 잔류 정책까지 적용했는지
        bool bCheckoutTimeHandled = false;
    };

    UFUNCTION()
    void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

    UFUNCTION()
    void HandleRoomAssigned(AGuestCharacter* Guest);

    void ProcessCheckouts(double CurrentSeconds);
    void TryAdmitGuest(double CurrentSeconds);

    bool HasAvailableRoom() const;

    TWeakObjectPtr<UGameClockSubsystem> Clock;

    // 아직 객실을 배정받지 않은 손님 한 명
    TWeakObjectPtr<AGuestCharacter> PendingCheckInGuest;

    // 이벤트 연결 해제용 목록
    TArray<TWeakObjectPtr<AGuestCharacter>> TrackedGuests;

    TArray<FCheckoutSchedule> CheckoutSchedules;

    int64 ArrivalDayIndex = -1;
    int32 AdmittedToday = 0;

    double NextArrivalSeconds = 0.0;
    
    // 미배정 손님의 접수 마감 처리
    void ProcessCheckInDeadline(double CurrentSeconds);

    // 현재 미배정 손님의 접수 마감 시각
    // 날짜까지 포함한 누적 게임 초
    double PendingCheckInDeadlineSeconds = 0.0;
    
};