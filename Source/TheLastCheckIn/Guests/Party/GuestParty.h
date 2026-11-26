#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Guests/Data/GuestDataTypes.h"
#include "Engine/TimerHandle.h"
#include "GuestParty.generated.h"

class AActor;
class AGuestCharacter;
class ARoomActor;
class UGuestParty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGuestPartyRoomAssigned,
    UGuestParty*, Party);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGuestPartyCheckedIn,
    UGuestParty*, Party);

UENUM(BlueprintType)
enum class EGuestPartyState : uint8
{
    CheckingIn,
    MovingToRoom,
    Staying,
    WaitingForCheckoutRequest,
    MovingToLobby,
    MovingToExit,
    Finished
};

UCLASS(BlueprintType)
class THELASTCHECKIN_API UGuestParty : public UObject
{
    GENERATED_BODY()

public:
    virtual UWorld* GetWorld() const override;

    static UGuestParty* Create(
        UObject* InOuter,
        const TArray<AGuestCharacter*>& InMembers);

    FGuid GetPartyId() const { return PartyId; }
    int32 GetSize() const { return Members.Num(); }

    AGuestCharacter* GetLeader() const;
    TArray<AGuestCharacter*> GetMembers() const;

    UFUNCTION(BlueprintPure, Category = "Guests|Party")
    FGuid GetStayId() const { return StayId; }

    UFUNCTION(BlueprintPure, Category = "Guests|Party")
    ARoomActor* GetAssignedRoom() const
    {
        return AssignedRoom.Get();
    }

    bool HasAssignedRoom() const
    {
        return AssignedRoom != nullptr;
    }

    bool IsCheckingIn() const
    {
        return State == EGuestPartyState::CheckingIn;
    }

    bool IsFinished() const
    {
        return State == EGuestPartyState::Finished;
    }

    bool CanCheckIn() const;
    bool CanAssignRoom(const ARoomActor* Room) const;

    UFUNCTION(BlueprintCallable, Category = "Guests|Party")
    bool TryAssignRoom(ARoomActor* Room);

    // 체크인을 마치고 투숙 중인 일행
    bool IsStaying() const
    {
        return State == EGuestPartyState::Staying;
    }

    bool CanRequestCheckout() const;

    UFUNCTION(BlueprintCallable, Category = "Guests|Party")
    bool RequestCheckout();

    // 일정 관리자가 등장 직후 설정
    void ConfigureVisit(
        double DeadlineSeconds,
        AActor* InLobbyTarget,
        AActor* InExitTarget);

    void ConfigureMovementRecovery(float TimeoutSeconds, int32 MaxRetries);
    void StopMovementMonitoring();

    double GetCheckoutTime() const
    {
        return CheckoutAtSeconds;
    }

    void SetCheckoutTime(double Seconds)
    {
        // 퇴실 시간은 한 번만 등록
        if (CheckoutAtSeconds < 0.0)
        {
            CheckoutAtSeconds = Seconds;
        }
    }

    // 시간 갱신마다 일행 전체의 진행 상태 처리
    void Advance(double CurrentSeconds);

    UPROPERTY(BlueprintAssignable, Category = "Guests|Party")
    FGuestPartyRoomAssigned OnRoomAssigned;

    UPROPERTY(BlueprintAssignable, Category = "Guests|Party")
    FGuestPartyCheckedIn OnCheckedIn;

    // 투숙 중이며 석식 출발 준비 중이 아닌지 확인한다. 개인 상태는 각 컴포넌트에서 검사한다.
    bool CanParticipateInAnomaly() const;

    //아직 사건에 묶여 있는 손님이 있는지 검사
    bool HasAnomalyParticipants() const;

    // 일행 전원이 객실에서 다른 활동 없이 대기 중인지 확인
    bool CanStartDinner() const;

    bool TryReserveDinnerDeparture();

    void ReleaseDinnerDeparture()
    {
        bDinnerDepartureReserved = false;
    }

private:
    bool AllMembersInState(EGuestState RequiredState) const;

    // 출발 준비 중이거나 식당 방문을 마치지 않은 구성원이 있는지 확인한다.
    bool HasDiningParticipants() const;

    void MoveMembersToRoom();
    void MoveMembersToLobby();
    void MoveMembersToExit();
    void CheckMovementProgress();
    void CancelVisitAfterMovementFailure();

    FTimerHandle MovementWatchdogTimer;
    EGuestPartyState MonitoredState = EGuestPartyState::CheckingIn;
    double MovementAttemptStartedAt = -1.0;
    float MovementTimeoutSeconds = 60.0f;
    int32 MaxMovementRetries = 3;
    int32 MovementRetryCount = 0;

    UPROPERTY(VisibleAnywhere, Category = "Guests|Party")
    FGuid PartyId;

    UPROPERTY()
    TArray<TWeakObjectPtr<AGuestCharacter>> Members;

    UPROPERTY(VisibleAnywhere, Category = "Guests|Party")
    EGuestPartyState State = EGuestPartyState::CheckingIn;

    UPROPERTY()
    TObjectPtr<ARoomActor> AssignedRoom;

    UPROPERTY()
    FGuid StayId;

    UPROPERTY()
    TObjectPtr<AActor> LobbyTarget;

    UPROPERTY()
    TObjectPtr<AActor> ExitTarget;

    EGuestCheckoutPolicy CheckoutPolicy = EGuestCheckoutPolicy::Random;

    float StayBehindChance = 0.0f;
    bool bWillStayBehind = false;

    double CheckInDeadlineSeconds = -1.0;
    double CheckoutAtSeconds = -1.0;

    // 좌석을 확보하는 동안 이상행동 참여와 퇴실을 보류한다.
    bool bDinnerDepartureReserved = false;
};
