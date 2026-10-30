#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Guests/Data/GuestDataTypes.h"
#include "TimerManager.h"
#include "GuestParty.generated.h"

class AActor;
class AGuestCharacter;
class ARoomActor;
class UGuestParty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGuestPartyRoomAssigned,
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

private:
    bool AllMembersInState(EGuestState RequiredState) const;

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
};