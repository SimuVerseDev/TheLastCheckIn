#include "Guests/Anomaly/GuestAnomaly.h"

#include "Guests/Anomaly/GuestAnomalyComponent.h"
#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Party/GuestParty.h"

#include "Night/Anomaly/AnomalyManagerSubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    template <typename T>
    //배열의 순서를 무작위로 섞는 함수
    void ShuffleItems(TArray<T>& Items)
    {
        for (int32 Index = Items.Num() - 1; Index > 0; --Index)
        {
            Items.Swap(Index, FMath::RandRange(0, Index));
        }
    }
}

AGuestAnomaly::AGuestAnomaly()
{
    PrimaryActorTick.bCanEverTick = false;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AGuestAnomaly::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    Clock = World->GetSubsystem<UGameClockSubsystem>();

    if (IsValid(Clock))
    {
        Clock->OnTimeAdvanced.AddUniqueDynamic(
            this,
            &AGuestAnomaly::HandleTimeAdvanced);
    }

    if (UAnomalyManagerSubsystem* Manager =
        World->GetSubsystem<UAnomalyManagerSubsystem>())
    {
        // 플레이 중 생성된 사건도 등록
        Manager->RegisterAnomaly(this);

        if (AnomalyId.IsNone() ||
            Manager->FindAnomaly(AnomalyId) != this)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Guest anomaly needs a unique AnomalyId: %s"),
                *GetName());
        }
    }

    bReady = true;
    ScheduleNextStart();
}

bool AGuestAnomaly::IsInsideStartWindow() const
{
    if (!bUseStartTimeWindow)
    {
        return true;
    }

    if (!IsValid(Clock))
    {
        return false;
    }

    const int32 CurrentMinute =
        Clock->GetHour() * 60 + Clock->GetMinute();

    const int32 StartMinute =
        FMath::Clamp(StartWindowMinute, 0, 1439);

    const int32 EndMinute =
        FMath::Clamp(EndWindowMinute, 0, 1439);

    // 같은 시각이면 하루 전체 허용
    if (StartMinute == EndMinute)
    {
        return true;
    }

    if (StartMinute < EndMinute)
    {
        return CurrentMinute >= StartMinute
            && CurrentMinute < EndMinute;
    }

    // 자정을 넘는 시간대
    return CurrentMinute >= StartMinute
        || CurrentMinute < EndMinute;
}

void AGuestAnomaly::CollectCandidates(TArray<UGuestAnomalyComponent*>& OutGuests, TArray<AActor*>& OutPoints) const
{
    OutGuests.Reset();
    OutPoints.Reset();

    UWorld* World = GetWorld();

    if (!World || !IsValid(Definition))
    {
        return;
    }

    for (const TObjectPtr<AActor>& PointRef : ActionPoints)
    {
        AActor* Point = PointRef.Get();

        if (!IsValid(Point) ||
            Point->IsActorBeingDestroyed() ||
            Point->GetWorld() != World)
        {
            continue;
        }

        const bool bReserved =
            Participants.ContainsByPredicate(
                [Point](const FGuestAnomalyParticipant& Participant)
                {
                    return Participant.ContextActor.Get() == Point;
                });

        if (!bReserved)
        {
            OutPoints.AddUnique(Point);
        }
    }

    if (OutPoints.IsEmpty())
    {
        return;
    }

    for (TActorIterator<AGuestCharacter> It(World); It; ++It)
    {
        AGuestCharacter* Guest = *It;

        if (!IsValid(Guest) || Guest->IsActorBeingDestroyed())
        {
            continue;
        }

        UGuestAnomalyComponent* Component =
            Guest->GetAnomalyComponent();

        if (!IsValid(Component) ||
            !Component->CanStartFor(Definition.Get(), OutPoints[0]))
        {
            continue;
        }

        const bool bAlreadyParticipating =
            Participants.ContainsByPredicate(
                [Component](const FGuestAnomalyParticipant& Participant)
                {
                    return Participant.Component.Get() == Component;
                });

        if (bAlreadyParticipating)
        {
            continue;
        }

        UGuestParty* Party = Guest->GetParty();

        if (bExcludeGuestsPastCheckout && IsValid(Party))
        {
            const double CheckoutTime = Party->GetCheckoutTime();

            if (CheckoutTime >= 0.0 &&
                (!IsValid(Clock) ||
                 Clock->GetTotalGameSeconds() >= CheckoutTime))
            {
                continue;
            }
        }

        OutGuests.Add(Component);
    }
}

bool AGuestAnomaly::CanStartAnomaly() const
{
    UWorld* World = GetWorld();

    if (!World ||
        !bReady ||
        bEndingPlay ||
        IsActorBeingDestroyed() ||
        bChangingIncident ||
        bEvaluationQueued ||
        IsRunning() ||
        !Participants.IsEmpty() ||
        AnomalyId.IsNone() ||
        !IsValid(Definition) ||
        !IsInsideStartWindow())
    {
        return false;
    }

    if (const UHotelPhaseSubsystem* Phase =
        World->GetSubsystem<UHotelPhaseSubsystem>())
    {
        if (Phase->GetCurrentPhase() == EHotelPhase::Settlement)
        {
            return false;
        }
    }

    const UAnomalyManagerSubsystem* Manager =
        World->GetSubsystem<UAnomalyManagerSubsystem>();

    if (!Manager || Manager->FindAnomaly(AnomalyId) != this)
    {
        return false;
    }

    TArray<UGuestAnomalyComponent*> Guests;
    TArray<AActor*> Points;
    CollectCandidates(Guests, Points);

    return !Guests.IsEmpty() && !Points.IsEmpty();
}

void AGuestAnomaly::BindParticipant(const FGuestAnomalyParticipant& Participant)
{
    if (UGuestAnomalyComponent* Component =
        Participant.Component.Get())
    {
        Component->OnResolved.AddUniqueDynamic(
            this,
            &AGuestAnomaly::HandleParticipantResolved);

        Component->OnCancelled.AddUniqueDynamic(
            this,
            &AGuestAnomaly::HandleParticipantCancelled);
    }

    if (AGuestCharacter* Guest = Participant.Guest.Get())
    {
        Guest->OnEndPlay.AddUniqueDynamic(
            this,
            &AGuestAnomaly::HandleParticipantActorEndPlay);
    }

    if (AActor* Point = Participant.ContextActor.Get())
    {
        Point->OnEndPlay.AddUniqueDynamic(
            this,
            &AGuestAnomaly::HandleParticipantActorEndPlay);
    }
}

void AGuestAnomaly::UnbindParticipant(const FGuestAnomalyParticipant& Participant)
{
    if (UGuestAnomalyComponent* Component =
        Participant.Component.Get())
    {
        Component->OnResolved.RemoveDynamic(
            this,
            &AGuestAnomaly::HandleParticipantResolved);

        Component->OnCancelled.RemoveDynamic(
            this,
            &AGuestAnomaly::HandleParticipantCancelled);
    }

    if (AGuestCharacter* Guest = Participant.Guest.Get())
    {
        Guest->OnEndPlay.RemoveDynamic(
            this,
            &AGuestAnomaly::HandleParticipantActorEndPlay);
    }

    if (AActor* Point = Participant.ContextActor.Get())
    {
        Point->OnEndPlay.RemoveDynamic(
            this,
            &AGuestAnomaly::HandleParticipantActorEndPlay);
    }
}

bool AGuestAnomaly::TryAddParticipant()
{
    if (Participants.Num() >= FMath::Max(1, MaxParticipants))
    {
        return false;
    }

    TArray<UGuestAnomalyComponent*> Guests;
    TArray<AActor*> Points;
    CollectCandidates(Guests, Points);

    ShuffleItems(Guests);
    ShuffleItems(Points);

    for (UGuestAnomalyComponent* Component : Guests)
    {
        for (AActor* Point : Points)
        {
            if (!IsValid(Component) ||
                !IsValid(Point) ||
                !Component->CanStartFor(Definition.Get(), Point))
            {
                continue;
            }

            if (!Component->StartAnomaly(Definition.Get(), Point))
            {
                continue;
            }

            FGuestAnomalyParticipant Participant;
            Participant.Guest =
                Cast<AGuestCharacter>(Component->GetOwner());

            Participant.Component = Component;
            Participant.ContextActor = Point;
            Participant.ParticipationId =
                Component->GetParticipationId();

            Participants.Add(Participant);
            BindParticipant(Participant);

            // 시작 중 즉시 해결된 경우도 확인
            QueueEvaluation();

            UE_LOG(
                LogTemp,
                Log,
                TEXT("Guest anomaly participant added: %s, total=%d"),
                *GetNameSafe(Component->GetOwner()),
                Participants.Num());

            return true;
        }
    }

    return false;
}

bool AGuestAnomaly::StartAnomaly()
{
    if (!CanStartAnomaly())
    {
        return false;
    }

    TGuardValue<bool> Guard(bChangingIncident, true);

    ClearEvaluation();

    // 첫 참여자의 실행이 성공해야 사건 활성화
    if (!TryAddParticipant())
    {
        return false;
    }

    if (!Super::StartAnomaly())
    {
        CleanupParticipants(false);
        return false;
    }

    if (bEndingPlay || IsActorBeingDestroyed() || !IsRunning())
    {
        CleanupParticipants(false);
        return false;
    }

    ScheduleNextSpread();
    QueueEvaluation();

    return true;
}

bool AGuestAnomaly::IsSameParticipation(const FGuestAnomalyParticipant& Participant) const
{
    const UGuestAnomalyComponent* Component =
        Participant.Component.Get();

    return IsValid(Component)
        && Participant.ParticipationId.IsValid()
        && Component->GetParticipationId() == Participant.ParticipationId;
}

bool AGuestAnomaly::AreParticipantsValid() const
{
    if (Participants.IsEmpty())
    {
        return false;
    }

    for (const FGuestAnomalyParticipant& Participant : Participants)
    {
        if (!Participant.Guest.IsValid() ||
            !Participant.ContextActor.IsValid() ||
            !IsSameParticipation(Participant))
        {
            return false;
        }

        if (Participant.Guest->IsActorBeingDestroyed() ||
            Participant.ContextActor->IsActorBeingDestroyed() ||
            !Participant.Component->IsBusy())
        {
            return false;
        }
    }

    return true;
}

bool AGuestAnomaly::AreAllParticipantsResolved() const
{
    if (!AreParticipantsValid())
    {
        return false;
    }

    for (const FGuestAnomalyParticipant& Participant : Participants)
    {
        if (Participant.Component->GetPhase() !=
            EGuestAnomalyPhase::Resolved)
        {
            return false;
        }
    }

    return true;
}

int32 AGuestAnomaly::GetParticipantCount() const
{
    return Participants.Num();
}

int32 AGuestAnomaly::GetResolvedParticipantCount() const
{
    int32 Count = 0;

    for (const FGuestAnomalyParticipant& Participant : Participants)
    {
        if (IsSameParticipation(Participant) &&
            Participant.Component->GetPhase() ==
                EGuestAnomalyPhase::Resolved)
        {
            ++Count;
        }
    }

    return Count;
}

void AGuestAnomaly::HandleParticipantResolved(UGuestAnomalyComponent* Component)
{
    QueueEvaluation();
}

void AGuestAnomaly::HandleParticipantCancelled(UGuestAnomalyComponent* Component)
{
    QueueEvaluation(true);
}

void AGuestAnomaly::HandleParticipantActorEndPlay(AActor* Actor,EEndPlayReason::Type EndPlayReason)
{
    QueueEvaluation(true);
}

void AGuestAnomaly::QueueEvaluation(bool bRequestFailure)
{
    UWorld* World = GetWorld();

    if (bEndingPlay || !World)
    {
        return;
    }

    bFailureRequested |= bRequestFailure;

    if (bEvaluationQueued)
    {
        return;
    }

    bEvaluationQueued = true;

    EvaluationTimer =
        World->GetTimerManager().SetTimerForNextTick(
            this,
            &AGuestAnomaly::EvaluateIncident);
}

void AGuestAnomaly::EvaluateIncident()
{
    EvaluationTimer.Invalidate();
    bEvaluationQueued = false;

    if (bEndingPlay)
    {
        return;
    }

    if (bChangingIncident)
    {
        QueueEvaluation();
        return;
    }

    if (!IsRunning())
    {
        bFailureRequested = false;
        return;
    }

    if (bFailureRequested || !AreParticipantsValid())
    {
        Fail();
        return;
    }

    if (AreAllParticipantsResolved())
    {
        Resolve();
    }
}

void AGuestAnomaly::ClearEvaluation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EvaluationTimer);
    }

    EvaluationTimer.Invalidate();
    bEvaluationQueued = false;
    bFailureRequested = false;
}

void AGuestAnomaly::CleanupParticipants(bool bResolved)
{
    ClearEvaluation();

    const UHotelPhaseSubsystem* HotelPhase = GetWorld()
        ? GetWorld()->GetSubsystem<UHotelPhaseSubsystem>()
        : nullptr;
    const bool bReturnAfterSettlement = !bResolved && !bEndingPlay
        && HotelPhase
        && HotelPhase->GetCurrentPhase() == EHotelPhase::Settlement;

    TArray<FGuestAnomalyParticipant> OldParticipants =
        MoveTemp(Participants);

    Participants.Reset();

    // 정리로 발생하는 취소 이벤트는 받지 않음
    for (const FGuestAnomalyParticipant& Participant : OldParticipants)
    {
        UnbindParticipant(Participant);
    }

    for (const FGuestAnomalyParticipant& Participant : OldParticipants)
    {
        // 다른 사건으로 바뀐 손님은 건드리지 않음
        if (!IsSameParticipation(Participant))
        {
            continue;
        }

        UGuestAnomalyComponent* Component =
            Participant.Component.Get();

        if (!Component->IsBusy())
        {
            continue;
        }

        if (bReturnAfterSettlement)
        {
            // 사건은 실패로 끝내되, 손님의 객실 복귀는 계속 진행한다.
            Component->ReturnToRoomAfterIncidentEnd();
        }
        else if (bResolved &&
            Component->GetPhase() == EGuestAnomalyPhase::Resolved)
        {
            Component->FinishIncident();
        }
        else
        {
            Component->CancelAnomaly();
        }
    }
}

void AGuestAnomaly::ScheduleNextStart()
{
    if (!IsValid(Clock) || !Clock->IsClockRunning())
    {
        NextStartGameSeconds = -1.0;
        return;
    }

    const float MinMinutes =
        FMath::Max(0.1f, MinStartIntervalGameMinutes);

    const float MaxMinutes =
        FMath::Max(MinMinutes, MaxStartIntervalGameMinutes);

    NextStartGameSeconds =
        Clock->GetTotalGameSeconds()
        + FMath::FRandRange(MinMinutes, MaxMinutes) * 60.0;
}

void AGuestAnomaly::ScheduleNextSpread()
{
    if (!IsValid(Clock) || !Clock->IsClockRunning())
    {
        NextSpreadGameSeconds = -1.0;
        return;
    }

    NextSpreadGameSeconds =
        Clock->GetTotalGameSeconds()
        + FMath::Max(0.1f, SpreadIntervalGameMinutes) * 60.0;
}

bool AGuestAnomaly::Resolve()
{
    if (bEndingPlay ||
        bChangingIncident ||
        bFailureRequested ||
        !IsRunning() ||
        !AreAllParticipantsResolved())
    {
        return false;
    }

    TGuardValue<bool> Guard(bChangingIncident, true);

    ClearEvaluation();

    if (!Super::Resolve())
    {
        return false;
    }

    NextSpreadGameSeconds = -1.0;
    CleanupParticipants(true);
    ScheduleNextStart();

    return true;
}

bool AGuestAnomaly::Fail()
{
    if (bEndingPlay || !IsRunning())
    {
        return false;
    }

    if (bChangingIncident)
    {
        QueueEvaluation(true);
        return false;
    }

    TGuardValue<bool> Guard(bChangingIncident, true);

    ClearEvaluation();

    if (!Super::Fail())
    {
        return false;
    }

    NextSpreadGameSeconds = -1.0;
    CleanupParticipants(false);
    ScheduleNextStart();

    return true;
}

void AGuestAnomaly::HandleTimeAdvanced(double PreviousSeconds,double CurrentSeconds)
{
    if (!bReady ||
        bEndingPlay ||
        bChangingIncident ||
        bEvaluationQueued ||
        !IsValid(Clock) ||
        !Clock->IsClockRunning())
    {
        return;
    }

    // 호텔 시계를 과거로 초기화한 경우
    if (CurrentSeconds < PreviousSeconds)
    {
        ScheduleNextStart();
        ScheduleNextSpread();
        return;
    }

    if (const UHotelPhaseSubsystem* Phase =
        GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
    {
        if (Phase->GetCurrentPhase() == EHotelPhase::Settlement)
        {
            if (IsRunning())
            {
                Fail();
            }

            return;
        }
    }

    if (IsRunning())
    {
        if (Participants.Num() >= FMath::Max(1, MaxParticipants))
        {
            return;
        }

        if (NextSpreadGameSeconds < 0.0)
        {
            ScheduleNextSpread();
            return;
        }

        if (CurrentSeconds < NextSpreadGameSeconds)
        {
            return;
        }

        ScheduleNextSpread();

        if (!bAllowSpreadOutsideWindow && !IsInsideStartWindow())
        {
            return;
        }

        // 추가 참여 직전 상태 재확인
        if (!AreParticipantsValid())
        {
            QueueEvaluation(true);
            return;
        }

        if (AreAllParticipantsResolved())
        {
            QueueEvaluation();
            return;
        }

        TGuardValue<bool> Guard(bChangingIncident, true);
        TryAddParticipant();
        return;
    }

    if (!bAutomaticStart)
    {
        return;
    }

    if (NextStartGameSeconds < 0.0)
    {
        ScheduleNextStart();
        return;
    }

    if (CurrentSeconds < NextStartGameSeconds)
    {
        return;
    }

    // 후보가 없어도 다음 시도를 예약
    ScheduleNextStart();

    if (!CanStartAnomaly())
    {
        return;
    }

    if (UAnomalyManagerSubsystem* Manager =
        GetWorld()->GetSubsystem<UAnomalyManagerSubsystem>())
    {
        Manager->StartAnomalyById(AnomalyId);
    }
}

void AGuestAnomaly::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    bReady = false;

    TGuardValue<bool> Guard(bChangingIncident, true);

    if (IsValid(Clock))
    {
        Clock->OnTimeAdvanced.RemoveDynamic(
            this,
            &AGuestAnomaly::HandleTimeAdvanced);
    }

    ClearEvaluation();

    // 플레이 중 사건 액터가 제거됨
    if (IsRunning() &&
        (EndPlayReason == EEndPlayReason::Destroyed ||
         EndPlayReason == EEndPlayReason::RemovedFromWorld))
    {
        Super::Fail();
    }

    CleanupParticipants(false);

    Super::EndPlay(EndPlayReason);
}
