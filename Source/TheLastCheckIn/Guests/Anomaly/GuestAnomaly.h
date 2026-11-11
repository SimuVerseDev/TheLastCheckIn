#pragma once

#include "CoreMinimal.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "TimerManager.h"
#include "GuestAnomaly.generated.h"

class AGuestCharacter;
class UGuestAnomalyComponent;
class UGuestAnomalyDefinition;
class UGameClockSubsystem;

// 한 손님의 사건 참여 기록
struct FGuestAnomalyParticipant
{
    TWeakObjectPtr<AGuestCharacter> Guest;
    TWeakObjectPtr<UGuestAnomalyComponent> Component;
    TWeakObjectPtr<AActor> ContextActor;
    FGuid ParticipationId;
};

UCLASS(Blueprintable)
class THELASTCHECKIN_API AGuestAnomaly : public AAnomalyBase
{
    GENERATED_BODY()

public:
    AGuestAnomaly();

    UFUNCTION(BlueprintPure, Category = "Guests|Anomaly")
    bool CanStartAnomaly() const;

    virtual bool StartAnomaly() override;
    virtual bool Resolve() override;
    virtual bool Fail() override;

    UFUNCTION(BlueprintPure, Category = "Guests|Anomaly")
    int32 GetParticipantCount() const;

    UFUNCTION(BlueprintPure, Category = "Guests|Anomaly")
    int32 GetResolvedParticipantCount() const;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    // 참여 손님에게 적용할 행동
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly")
    TObjectPtr<UGuestAnomalyDefinition> Definition;

    // 같은 사건 안에서는 중복 배정하지 않음
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guests|Anomaly")
    TArray<TObjectPtr<AActor>> ActionPoints;

    // 제압된 손님도 포함한 전체 참여 인원
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly",
        meta = (ClampMin = "1"))
    int32 MaxParticipants = 4;

    // 호텔 시간 기준 추가 참여 간격
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Spread",
        meta = (ClampMin = "0.1", Units = "min"))
    float SpreadIntervalGameMinutes = 10.0f;

    // 퇴실 시각이 지난 손님은 새 참여에서 제외
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly")
    bool bExcludeGuestsPastCheckout = true;

    // 첫 테스트는 수동으로 시작
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Schedule")
    bool bAutomaticStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Schedule",
        meta = (ClampMin = "0.1", Units = "min"))
    float MinStartIntervalGameMinutes = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Schedule",
        meta = (ClampMin = "0.1", Units = "min"))
    float MaxStartIntervalGameMinutes = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Schedule")
    bool bUseStartTimeWindow = false;

    // 자정부터 지난 분: 22시 = 1320
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Schedule",
        meta = (ClampMin = "0", ClampMax = "1439"))
    int32 StartWindowMinute = 1320;

    // 종료 시각 미포함: 05시 = 300
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Schedule",
        meta = (ClampMin = "0", ClampMax = "1439"))
    int32 EndWindowMinute = 300;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guests|Anomaly|Spread")
    bool bAllowSpreadOutsideWindow = true;

private:
    void CollectCandidates(TArray<UGuestAnomalyComponent*>& OutGuests, TArray<AActor*>& OutPoints) const;

    bool TryAddParticipant();

    bool IsSameParticipation(
        const FGuestAnomalyParticipant& Participant) const;

    bool AreParticipantsValid() const;
    bool AreAllParticipantsResolved() const;
    bool IsInsideStartWindow() const;

    void BindParticipant(const FGuestAnomalyParticipant& Participant);

    void UnbindParticipant(const FGuestAnomalyParticipant& Participant);

    // 콜백이 끝난 뒤 한 번만 검사
    void QueueEvaluation(bool bRequestFailure = false);
    void EvaluateIncident();
    void ClearEvaluation();

    void CleanupParticipants(bool bResolved);
    void ScheduleNextStart();
    void ScheduleNextSpread();

    UFUNCTION()
    void HandleParticipantResolved(UGuestAnomalyComponent* Component);

    UFUNCTION()
    void HandleParticipantCancelled(UGuestAnomalyComponent* Component);

    UFUNCTION()
    void HandleParticipantActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

    UFUNCTION()
    void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

    TArray<FGuestAnomalyParticipant> Participants;

    UPROPERTY(Transient)
    TObjectPtr<UGameClockSubsystem> Clock;

    FTimerHandle EvaluationTimer;

    double NextStartGameSeconds = -1.0;
    double NextSpreadGameSeconds = -1.0;

    bool bReady = false;
    bool bChangingIncident = false;
    bool bEndingPlay = false;
    bool bEvaluationQueued = false;
    bool bFailureRequested = false;
};