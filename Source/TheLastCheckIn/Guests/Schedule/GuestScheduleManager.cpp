#include "Guests/Schedule/GuestScheduleManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

#include "Core/Time/GameClockSubsystem.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Pool/GuestPoolManager.h"
#include "Guests/Party/GuestParty.h"
#include "Rooms/RoomActor.h"

AGuestScheduleManager::AGuestScheduleManager()
{
    PrimaryActorTick.bCanEverTick = false;

    SetRootComponent(
        CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void AGuestScheduleManager::BeginPlay()
{
    Super::BeginPlay();

    Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>();

    if (!Clock.IsValid() ||
        !IsValid(GuestPool.Get()) ||
        !IsValid(SpawnPoint.Get()) ||
        !IsValid(CounterTarget.Get()) ||
        !IsValid(LobbyTarget.Get()) ||
        !IsValid(ExitTarget.Get()) ||
        Rooms.IsEmpty() ||
        GuestTypeRow.IsNull())
    {
        return;
    }

    if (CheckInStartHour >= CheckInEndHour ||
        CheckoutStartHour > CheckoutEndHour)
    {
        return;
    }

    Clock->OnTimeAdvanced.AddUniqueDynamic(
        this,
        &AGuestScheduleManager::HandleTimeAdvanced);

    TArray<AActor*> SpawnPoints;
    SpawnPoints.Add(SpawnPoint.Get());
    for (const TObjectPtr<AActor>& Point : CompanionSpawnPoints)
    {
        SpawnPoints.Add(Point.Get());
    }
    const int32 MaxSize = FMath::Clamp(MaxPartySize, FMath::Clamp(MinPartySize, 1, 4), 4);
    bArrivalsDisabled = !GuestPool->ValidatePartyConfiguration(
        SpawnPoints, CounterTarget.Get(), GuestTypeRow, MaxSize);
    if (bArrivalsDisabled)
    {
        UE_LOG(LogTemp, Error, TEXT("GuestSchedule: Arrivals disabled due to configuration errors. Fix the settings and restart play."));
    }
}

void AGuestScheduleManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Clock.IsValid())
    {
        Clock->OnTimeAdvanced.RemoveDynamic(
            this,
            &AGuestScheduleManager::HandleTimeAdvanced);
    }

    for (const TObjectPtr<UGuestParty>& Party : ActiveParties)
    {
        if (IsValid(Party.Get()))
        {
            Party->StopMovementMonitoring();
            Party->OnRoomAssigned.RemoveDynamic(
                this,
                &AGuestScheduleManager::HandlePartyRoomAssigned);
        }
    }

    ActiveParties.Reset();
    NextPartySize = 0;

    Super::EndPlay(EndPlayReason);
}

void AGuestScheduleManager::HandleTimeAdvanced(
    double PreviousSeconds,
    double CurrentSeconds)
{
    for (int32 Index = ActiveParties.Num() - 1;
         Index >= 0;
         --Index)
    {
        UGuestParty* Party = ActiveParties[Index].Get();

        if (!IsValid(Party))
        {
            ActiveParties.RemoveAtSwap(Index);
            continue;
        }

        Party->Advance(CurrentSeconds);

        if (Party->IsFinished())
        {
            Party->OnRoomAssigned.RemoveDynamic(
                this,
                &AGuestScheduleManager::HandlePartyRoomAssigned);

            ActiveParties.RemoveAtSwap(Index);
        }
    }

    TryAdmitGuest(CurrentSeconds);
}

bool AGuestScheduleManager::HasAvailableRoom() const
{
    for (const TObjectPtr<ARoomActor>& Room : Rooms)
    {
        if (IsValid(Room.Get()) &&
            Room->GetRoomState() == ERoomState::Available &&
            Room->GetCapacity() > 0 &&
            IsValid(Room->GetArrivalTarget()))
        {
            return true;
        }
    }

    return false;
}



void AGuestScheduleManager::TryAdmitGuest(double CurrentSeconds)
{
    if (bArrivalsDisabled || !IsValid(GuestPool.Get()) ||
        !IsValid(SpawnPoint.Get()) ||
        !IsValid(CounterTarget.Get()))
    {
        return;
    }

    const int64 DayIndex =
        FMath::FloorToInt64(CurrentSeconds / 86400.0);

    if (ArrivalDayIndex != DayIndex)
    {
        ArrivalDayIndex = DayIndex;
        AdmittedToday = 0;
        NextArrivalSeconds = CurrentSeconds;
        NextPartySize = 0;
        SpawnFailureCount = 0;
    }

    const double SecondsOfDay =
        CurrentSeconds - static_cast<double>(DayIndex) * 86400.0;

    const bool bReceptionOpen =
        SecondsOfDay >= CheckInStartHour * 3600.0 &&
        SecondsOfDay < CheckInEndHour * 3600.0;

    if (!bReceptionOpen ||
        AdmittedToday >= DailyGuestLimit ||
        CurrentSeconds < NextArrivalSeconds)
    {
        return;
    }

    // 접수 중인 일행이 있으면 다음 일행은 대기
    for (const TObjectPtr<UGuestParty>& Party : ActiveParties)
    {
        if (IsValid(Party.Get()) && Party->IsCheckingIn())
        {
            return;
        }
    }

    if (!HasAvailableRoom())
    {
        return;
    }

    const int32 MinSize = FMath::Clamp(MinPartySize, 1, 4);
    const int32 MaxSize = FMath::Clamp(MaxPartySize, MinSize, 4);

    const int32 RemainingGuests = DailyGuestLimit - AdmittedToday;

    // 남은 입장 가능 인원이 최소 일행 인원보다 적으면 종료
    if (RemainingGuests < MinSize)
    {
        return;
    }

    // 새 일행의 인원을 한 번만 결정
    if (NextPartySize == 0)
    {
        NextPartySize = FMath::RandRange(
            MinSize,
            FMath::Min(MaxSize, RemainingGuests));
    }

    if (NextPartySize > RemainingGuests)
    {
        return;
    }

    TArray<AActor*> SpawnPoints;
    SpawnPoints.Add(SpawnPoint.Get());

    for (const TObjectPtr<AActor>& Point : CompanionSpawnPoints)
    {
        SpawnPoints.Add(Point.Get());
    }

    TArray<AGuestCharacter*> Members;

    const EGuestAcquireResult Result = GuestPool->TryAcquireParty(
            SpawnPoints,
            CounterTarget.Get(),
            GuestTypeRow,
            NextPartySize,
            Members);
    if (Result != EGuestAcquireResult::Success)
    {
        if (Result == EGuestAcquireResult::InvalidConfiguration)
        {
            bArrivalsDisabled = true;
            UE_LOG(LogTemp, Error, TEXT("GuestSchedule: Arrivals disabled; see GuestPool configuration error above."));
            return;
        }

        NextArrivalSeconds = CurrentSeconds + FMath::Max(1, SpawnRetryIntervalMinutes) * 60.0;
        // 손님이 모두 사용 중이면 같은 인원을 유지하고 반환을 기다림.
        if (Result == EGuestAcquireResult::PoolExhausted)
        {
            return;
        }

        ++SpawnFailureCount;

        if (SpawnFailureCount > FMath::Max(0, MaxSpawnRetries))
        {
            NextPartySize = 0;
            SpawnFailureCount = 0;
            const int32 MinMinutes = FMath::Max(1, MinArrivalIntervalMinutes);
            NextArrivalSeconds = CurrentSeconds + FMath::RandRange(
                MinMinutes, FMath::Max(MinMinutes, MaxArrivalIntervalMinutes)) * 60.0;
        }
        return;
    }

    UGuestParty* NewParty = Members[0]->GetParty();

    const double DeadlineSeconds =
        static_cast<double>(DayIndex) * 86400.0
        + CheckInEndHour * 3600.0;

    NewParty->ConfigureMovementRecovery(MovementTimeoutSeconds, MaxMovementRetries);
    NewParty->ConfigureVisit(
        DeadlineSeconds,
        LobbyTarget.Get(),
        ExitTarget.Get());

    NewParty->OnRoomAssigned.AddUniqueDynamic(
        this,
        &AGuestScheduleManager::HandlePartyRoomAssigned);

    ActiveParties.Add(NewParty);

    // 팀 수가 아니라 실제 등장한 사람 수를 누적
    AdmittedToday += Members.Num();

    NextPartySize = 0;
    SpawnFailureCount = 0;

    const int32 MinMinutes =
        FMath::Max(1, MinArrivalIntervalMinutes);

    const int32 MaxMinutes =
        FMath::Max(MinMinutes, MaxArrivalIntervalMinutes);

    const int32 IntervalMinutes =
        FMath::RandRange(MinMinutes, MaxMinutes);

    NextArrivalSeconds =
        CurrentSeconds + IntervalMinutes * 60.0;
}

void AGuestScheduleManager::HandlePartyRoomAssigned(UGuestParty* Party)
{
    if (!IsValid(Party) ||
        !Clock.IsValid() ||
        Party->GetCheckoutTime() >= 0.0)
    {
        return;
    }

    const double Now = Clock->GetTotalGameSeconds();

    const int64 CheckInDayIndex =
        FMath::FloorToInt64(Now / 86400.0);

    const double NextDayStart =
        static_cast<double>(CheckInDayIndex + 1) * 86400.0;

    const int32 CheckoutMinuteOfDay = FMath::RandRange(
        CheckoutStartHour * 60,
        CheckoutEndHour * 60);

    Party->SetCheckoutTime(NextDayStart + CheckoutMinuteOfDay * 60.0);
}
