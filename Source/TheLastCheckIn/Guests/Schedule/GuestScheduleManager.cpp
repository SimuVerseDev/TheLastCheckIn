#include "Guests/Schedule/GuestScheduleManager.h"
#include "TheLastCheckIn.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Core/GameFlow/HotelPhaseSettings.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Pool/GuestPoolManager.h"
#include "Guests/Party/GuestParty.h"
#include "Rooms/RoomActor.h"
#include "Tutorial/TutorialSubsystem.h"
#include "FoodService/Buffet/BuffetStation.h"
#include "FoodService/GuestDiningComponent.h"
#include "FoodService/Seats/DiningSeat.h"
#include "FoodService/Data/FoodDefinition.h"

AGuestScheduleManager::AGuestScheduleManager()
{
    PrimaryActorTick.bCanEverTick = false;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
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

    if (!UHotelPhaseSettings::Get()->HasValidHourOrder())
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
        UE_LOG(LogGuests, Error, TEXT("GuestSchedule: Arrivals disabled due to configuration errors. Fix the settings and restart play."));
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
            Party->OnCheckedIn.RemoveDynamic(
                this,
                &AGuestScheduleManager::HandlePartyCheckedIn);
        }
    }

    ActiveParties.Reset();
    DinnerPlans.Reset();
    DinnerPlanDay = -1;
    NextPartySize = 0;

    Super::EndPlay(EndPlayReason);
}

void AGuestScheduleManager::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
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
            Party->OnCheckedIn.RemoveDynamic(
                this,
                &AGuestScheduleManager::HandlePartyCheckedIn);

            ActiveParties.RemoveAtSwap(Index);
        }
    }

    // 기존 방문의 착석과 복귀를 먼저 처리
    UpdateDining(CurrentSeconds);

    // 객실 복귀가 반영된 인원으로 다음 석식 출발을 결정
    UpdateDinner(CurrentSeconds);

    TryAdmitGuest(CurrentSeconds);
}

bool AGuestScheduleManager::HasAvailableRoom() const
{
    for (const TObjectPtr<ARoomActor>& Room : Rooms)
    {
        if (IsValid(Room.Get()) &&
            Room->IsUnlocked() &&
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
        GameTime::GetDayIndex(CurrentSeconds);

    if (ArrivalDayIndex != DayIndex)
    {
        ArrivalDayIndex = DayIndex;
        AdmittedToday = 0;
        NextArrivalSeconds = CurrentSeconds;
        NextPartySize = 0;
        SpawnFailureCount = 0;
    }

    const double SecondsOfDay =
        CurrentSeconds - GameTime::GetDayStartSeconds(DayIndex);
    
    const UHotelPhaseSettings* PhaseSettings = UHotelPhaseSettings::Get();

    const bool bReceptionOpen =
        SecondsOfDay >= GameTime::HoursToSeconds(PhaseSettings->CheckInStartHour) &&
        SecondsOfDay < GameTime::HoursToSeconds(PhaseSettings->EveningStartHour);

    int32 EffectiveGuestLimit = DailyGuestLimit;
    if (const UTutorialSubsystem* Tutorial = GetWorld()->GetSubsystem<UTutorialSubsystem>();
        Tutorial && Tutorial->IsTutorialActive()
        && (Tutorial->GetCurrentStep() == ETutorialStep::CleanRoom
            || Tutorial->GetCurrentStep() == ETutorialStep::UnlockRoom205
            || Tutorial->GetCurrentStep() == ETutorialStep::CheckInGuest)
        && DailyGuestLimit > 0)
    {
        // DailyGuestLimit counts people; the tutorial requires five whole parties.
        EffectiveGuestLimit = FMath::Max(DailyGuestLimit, 5 * FMath::Clamp(MaxPartySize, 1, 4));
    }

    if (!bReceptionOpen ||
        AdmittedToday >= EffectiveGuestLimit ||
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

    const int32 RemainingGuests = EffectiveGuestLimit - AdmittedToday;

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
            UE_LOG(LogGuests, Error, TEXT("GuestSchedule: Arrivals disabled; see GuestPool configuration error above."));
            return;
        }

        NextArrivalSeconds = CurrentSeconds + GameTime::MinutesToSeconds(FMath::Max(1, SpawnRetryIntervalMinutes));
        // 손님이 모두 사용 중이면 같은 인원을 유지하고 반환을 기다림.
        if (Result == EGuestAcquireResult::PoolExhausted ||
            Result == EGuestAcquireResult::CounterBusy)
        {
            return;
        }

        ++SpawnFailureCount;

        if (SpawnFailureCount > FMath::Max(0, MaxSpawnRetries))
        {
            NextPartySize = 0;
            SpawnFailureCount = 0;
            const int32 MinMinutes = FMath::Max(1, MinArrivalIntervalMinutes);
            NextArrivalSeconds = CurrentSeconds + GameTime::MinutesToSeconds(FMath::RandRange(
                MinMinutes, FMath::Max(MinMinutes, MaxArrivalIntervalMinutes)));
        }
        return;
    }

    UGuestParty* NewParty = Members[0]->GetParty();

    const double DeadlineSeconds =
        GameTime::GetDayStartSeconds(DayIndex)
        + GameTime::HoursToSeconds(PhaseSettings->EveningStartHour);

    NewParty->ConfigureMovementRecovery(MovementTimeoutSeconds, MaxMovementRetries);
    NewParty->ConfigureVisit(
        DeadlineSeconds,
        LobbyTarget.Get(),
        ExitTarget.Get());

    NewParty->OnRoomAssigned.AddUniqueDynamic(
        this,
        &AGuestScheduleManager::HandlePartyRoomAssigned);
    
    NewParty->OnCheckedIn.AddUniqueDynamic(
        this,
        &AGuestScheduleManager::HandlePartyCheckedIn);

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
        CurrentSeconds + GameTime::MinutesToSeconds(IntervalMinutes);
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
        GameTime::GetDayIndex(Now);

    const double NextDayStart =
        GameTime::GetDayStartSeconds(CheckInDayIndex + 1);

    const UHotelPhaseSettings* PhaseSettings = UHotelPhaseSettings::Get();

    const int32 CheckoutMinuteOfDay = FMath::RandRange(
        9 * 60,
        PhaseSettings->MiddayStartHour * 60);

    Party->SetCheckoutTime(NextDayStart + GameTime::MinutesToSeconds(CheckoutMinuteOfDay));
}

void AGuestScheduleManager::HandlePartyCheckedIn(UGuestParty* Party)
{
    if (!IsValid(Party))
    {
        return;
    }

    ARoomActor* Room = Party->GetAssignedRoom();
    UHotelEconomySubsystem* Economy =
        GetWorld()->GetSubsystem<UHotelEconomySubsystem>();
    if (IsValid(Room) && IsValid(Economy))
    {
        const UHotelReputationSubsystem* Reputation = GetWorld()->GetSubsystem<UHotelReputationSubsystem>();
        const float Multiplier = Reputation ? Reputation->GetRoomRateMultiplier() : 1.0f;
        /*
        인당 부과 * 성급 배율
        */
        Economy->AddIncome(Room->GetBaseRoomRate() * Party->GetSize() * Multiplier);
    }
}

// 식당 방문 진행과 조식 출발
void AGuestScheduleManager::UpdateDining(double CurrentSeconds)
{
    const double SecondsOfDay = GameTime::GetSecondsOfDay(CurrentSeconds);
    const double StartSeconds = GameTime::HoursToSeconds(BreakfastStartHour);
    const double EndSeconds = GameTime::HoursToSeconds(BreakfastEndHour);

    const bool bBreakfastOpen =
        bEnableBreakfast
        && StartSeconds < EndSeconds
        && SecondsOfDay >= StartSeconds
        && SecondsOfDay < EndSeconds;

    TArray<AGuestCharacter*> Guests;

    for (const TObjectPtr<UGuestParty>& Party : ActiveParties)
    {
        if (!IsValid(Party.Get()))
        {
            continue;
        }

        for (AGuestCharacter* Guest : Party->GetMembers())
        {
            if (IsValid(Guest) && !Guest->IsActorBeingDestroyed())
            {
                Guests.AddUnique(Guest);
            }
        }
    }

    // 영업 종료 후에도 대기 종료와 객실 복귀를 처리한다.
    for (AGuestCharacter* Guest : Guests)
    {
        if (!IsValid(Guest) || Guest->IsActorBeingDestroyed())
        {
            continue;
        }

        UGuestDiningComponent* Dining =
            Guest->GetDiningComponent();

        if (IsValid(Dining))
        {
            Dining->AdvanceDining(CurrentSeconds, bBreakfastOpen);
        }
    }

    if (!bBreakfastOpen)
    {
        return;
    }

    // 매번 같은 손님이 먼저 예약하지 않도록 순서를 섞는다.
    for (int32 Index = Guests.Num() - 1; Index > 0; --Index)
    {
        Guests.Swap(Index, FMath::RandRange(0, Index));
    }

    for (AGuestCharacter* Guest : Guests)
    {
        if (!IsValid(Guest) || Guest->IsActorBeingDestroyed())
        {
            continue;
        }

        UGuestDiningComponent* Dining =
            Guest->GetDiningComponent();

        if (IsValid(Dining))
        {
            Dining->TryStartBreakfast(BreakfastBuffets, DiningSeats, CurrentSeconds);
        }
    }
}

// 석식 일정과 일행 교대
void AGuestScheduleManager::UpdateDinner(double CurrentSeconds)
{
    if (!bEnableDinner
        || DinnerStartHour >= DinnerEndHour
        || DinnerMenu.IsEmpty())
    {
        return;
    }

    const int64 DayIndex = GameTime::GetDayIndex(CurrentSeconds);

    if (DinnerPlanDay != DayIndex)
    {
        DinnerPlans.Reset();
        DinnerPlanDay = DayIndex;
    }

    const double DayStart = GameTime::GetDayStartSeconds(DayIndex);

    const double StartSeconds = DayStart + GameTime::HoursToSeconds(DinnerStartHour);

    const double EndSeconds = DayStart + GameTime::HoursToSeconds(DinnerEndHour);

    if (CurrentSeconds >= EndSeconds)
    {
        return;
    }

    // 입실한 일행은 석식 시작 시각부터 출발한다. 시작 후 입실하면 바로 출발을 시도한다.
    for (const TObjectPtr<UGuestParty>& PartyRef : ActiveParties)
    {
        UGuestParty* Party = PartyRef.Get();

        if (!IsValid(Party)
            || !Party->IsStaying()
            || !Party->GetStayId().IsValid()
            || DinnerPlans.Contains(Party->GetStayId()))
        {
            continue;
        }

        FGuestDinnerPlan Plan;
        Plan.ScheduledSeconds = FMath::Max(CurrentSeconds, StartSeconds);

        DinnerPlans.Add(Party->GetStayId(), Plan);
    }

    if (CurrentSeconds < StartSeconds)
    {
        return;
    }

    const double WorldNow = GetWorld()->GetTimeSeconds();

    TArray<UGuestParty*> Candidates;

    for (const TObjectPtr<UGuestParty>& PartyRef : ActiveParties)
    {
        UGuestParty* Party = PartyRef.Get();

        if (!IsDinnerCandidate(Party, CurrentSeconds))
        {
            continue;
        }

        const FGuestDinnerPlan* Plan =
            DinnerPlans.Find(Party->GetStayId());

        if (!Plan
            || Plan->bStarted
            || CurrentSeconds < Plan->ScheduledSeconds
            || WorldNow < Plan->NextAttemptWorldSeconds)
        {
            continue;
        }

        Candidates.Add(Party);
    }

    Candidates.Sort(
        [this](const UGuestParty& A, const UGuestParty& B)
        {
            return DinnerPlans.FindChecked(A.GetStayId()).ScheduledSeconds
                < DinnerPlans.FindChecked(B.GetStayId()).ScheduledSeconds;
        });

    const int32 MinimumRoomGuests =
        FMath::Max(1, MinRoomGuestsForAnomaly);

    for (UGuestParty* Party : Candidates)
    {
        if (!IsValid(Party) || !Party->CanStartDinner())
        {
            continue;
        }

        // 앞 일행의 출발 결과를 반영해서 매번 다시 센다.
        const int32 RoomGuests =
            CountAvailableRoomGuests(CurrentSeconds);

        if (RoomGuests <= MinimumRoomGuests)
        {
            continue;
        }

        FGuestDinnerPlan& Plan =
            DinnerPlans.FindChecked(Party->GetStayId());

        // 좌석이나 이동 준비가 실패하면 월드 시간 5초 뒤 재시도한다.
        Plan.NextAttemptWorldSeconds = WorldNow + 5.0;

        if (TryStartPartyDinner(Party, CurrentSeconds))
        {
            // 일부만 출발했다면 복귀 후 남은 손님도 다시 보낸다.
            Plan.bStarted = !Party->GetMembers().ContainsByPredicate(
                [CurrentSeconds](const AGuestCharacter* Member)
                {
                    const UGuestDiningComponent* Dining =
                        IsValid(Member) ? Member->GetDiningComponent() : nullptr;
                    return !IsValid(Dining) || !Dining->HasVisitedDinner(CurrentSeconds);
                });
        }
    }
}

// 석식 출발 조건과 대기 인원
bool AGuestScheduleManager::IsDinnerCandidate(
    const UGuestParty* Party,
    double CurrentSeconds) const
{
    if (!IsValid(Party) || !Party->CanStartDinner())
    {
        return false;
    }

    const double CheckoutTime = Party->GetCheckoutTime();
    return CheckoutTime < 0.0 || CurrentSeconds < CheckoutTime;
}

int32 AGuestScheduleManager::CountAvailableRoomGuests(
    double CurrentSeconds) const
{
    int32 Count = 0;

    for (const TObjectPtr<UGuestParty>& PartyRef : ActiveParties)
    {
        const UGuestParty* Party = PartyRef.Get();

        if (!IsValid(Party) || !Party->CanParticipateInAnomaly())
        {
            continue;
        }

        // 같은 일행이 식당에 있어도 객실에 남아 있는 손님은 따로 센다.
        // 이미 식사를 마치고 돌아온 손님도 대기 인원에 포함한다.
        for (const AGuestCharacter* Member : Party->GetMembers())
        {
            const UGuestDiningComponent* Dining =
                IsValid(Member) ? Member->GetDiningComponent() : nullptr;

            if (IsValid(Dining) && Dining->IsAvailableForDining(CurrentSeconds))
            {
                ++Count;
            }
        }
    }

    return Count;
}

// 객실 대기 인원을 남기고, 아직 석식에 가지 않은 구성원부터 보낸다.
bool AGuestScheduleManager::TryStartPartyDinner(UGuestParty* Party, double CurrentSeconds)
{
    const int32 DepartureLimit = CountAvailableRoomGuests(CurrentSeconds)
        - FMath::Max(1, MinRoomGuestsForAnomaly);

    if (DepartureLimit <= 0 || !IsValid(Party) || !Party->TryReserveDinnerDeparture())
    {
        return false;
    }

    TArray<UGuestDiningComponent*> PreparedGuests;

    const TArray<AGuestCharacter*> Members = Party->GetMembers();

    if (Members.IsEmpty() || Members.Num() != Party->GetSize())
    {
        Party->ReleaseDinnerDeparture();
        return false;
    }

    // 출발 가능 인원만 준비한다. 좌석을 못 잡은 손님은 다음 차례에 다시 시도한다.
    for (AGuestCharacter* Member : Members)
    {
        if (PreparedGuests.Num() >= DepartureLimit)
        {
            break;
        }

        UGuestDiningComponent* Dining =
            IsValid(Member) ? Member->GetDiningComponent() : nullptr;

        if (!IsValid(Dining)
            || !Dining->PrepareDinner(
                DiningSeats,
                DinnerMenu,
                CurrentSeconds))
        {
            continue;
        }

        PreparedGuests.Add(Dining);
    }

    bool bAnyStarted = false;

    for (UGuestDiningComponent* Dining : PreparedGuests)
    {
        if (!Dining->StartPreparedDinner())
        {
            // 실패한 손님의 예약만 해제하고, 다른 손님의 출발은 유지한다.
            Dining->CancelDinnerDeparture();
            continue;
        }

        bAnyStarted = true;
    }

    // 이제 각 손님의 식당 방문 상태가 이상행동 중복 참여를 막는다.
    Party->ReleaseDinnerDeparture();
    return bAnyStarted;
}
