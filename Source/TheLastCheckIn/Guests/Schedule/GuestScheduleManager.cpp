#include "Guests/Schedule/GuestScheduleManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

#include "Core/Time/GameClockSubsystem.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Pool/GuestPoolManager.h"
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
        UE_LOG(LogTemp, Error,
            TEXT("GuestSchedule: References, rooms or guest data are missing."));
        return;
    }

    if (CheckInStartHour >= CheckInEndHour ||
        CheckoutStartHour > CheckoutEndHour)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GuestSchedule: Invalid schedule hours."));
        return;
    }

    Clock->OnTimeAdvanced.AddUniqueDynamic(
        this,
        &AGuestScheduleManager::HandleTimeAdvanced);
}

void AGuestScheduleManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Clock.IsValid())
    {
        Clock->OnTimeAdvanced.RemoveDynamic(
            this,
            &AGuestScheduleManager::HandleTimeAdvanced);
    }

    for (const TWeakObjectPtr<AGuestCharacter>& WeakGuest : TrackedGuests)
    {
        if (AGuestCharacter* Guest = WeakGuest.Get())
        {
            Guest->OnGuestRoomAssigned.RemoveDynamic(
                this,
                &AGuestScheduleManager::HandleRoomAssigned);
        }
    }

    CheckoutSchedules.Reset();
    TrackedGuests.Reset();
    PendingCheckInGuest.Reset();
    PendingCheckInDeadlineSeconds = 0.0;

    Super::EndPlay(EndPlayReason);
}

void AGuestScheduleManager::HandleTimeAdvanced(
    double PreviousSeconds,
    double CurrentSeconds)
{
    // 미배정 손님은 접수 마감 시 귀가
    ProcessCheckInDeadline(CurrentSeconds);
    
    // 날짜가 바뀌어도 이전 날의 투숙, 퇴실 일정은 유지
    ProcessCheckouts(CurrentSeconds);

    // 신규 등장만 당일 접수 시간으로 판정
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

void AGuestScheduleManager::ProcessCheckInDeadline(double CurrentSeconds)
{
    AGuestCharacter* Guest = PendingCheckInGuest.Get();

    if (!IsValid(Guest))
    {
        PendingCheckInGuest.Reset();
        PendingCheckInDeadlineSeconds = 0.0;
        return;
    }

    if (CurrentSeconds < PendingCheckInDeadlineSeconds)
    {
        return;
    }

    if (Guest->TryLeaveWithoutCheckIn(ExitTarget.Get()))
    {
        // 출구 이동을 시작했으므로 접수 대기 목록에서 제거
        PendingCheckInGuest.Reset();
        PendingCheckInDeadlineSeconds = 0.0;
    }

    // 실패하면 참조를 유지하여 다음 시간 이벤트에서 재시도
}

void AGuestScheduleManager::TryAdmitGuest(double CurrentSeconds)
{
    if (!IsValid(GuestPool.Get()) ||
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

        // 기존 투숙 일정과 미배정 손님은 초기화하지 않음
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

    // 이전 손님이 객실을 배정받기 전까지 다음 손님을 보내지 않음
    if (PendingCheckInGuest.IsValid())
    {
        return;
    }

    if (!HasAvailableRoom())
    {
        return;
    }

    AGuestCharacter* Guest = GuestPool->AcquireGuest(
        SpawnPoint.Get(),
        CounterTarget.Get(),
        GuestTypeRow);

    if (!IsValid(Guest))
    {
        // 풀 부족이나 스폰 위치 막힘:
        // 이번 입장 시도는 인원에 포함하지 않고 나중에 재시도
        NextArrivalSeconds = CurrentSeconds + 60.0;
        return;
    }

    Guest->OnGuestRoomAssigned.AddUniqueDynamic(
        this,
        &AGuestScheduleManager::HandleRoomAssigned);

    TrackedGuests.AddUnique(TWeakObjectPtr<AGuestCharacter>(Guest));
    PendingCheckInGuest = Guest;
    
    PendingCheckInDeadlineSeconds =
    static_cast<double>(DayIndex) * 86400.0
    + CheckInEndHour * 3600.0;

    ++AdmittedToday;

    // 잘못된 설정값 보정
    const int32 MinMinutes =
        FMath::Max(1, MinArrivalIntervalMinutes);

    const int32 MaxMinutes =
        FMath::Max(MinMinutes, MaxArrivalIntervalMinutes);

    // 등장에 성공했을 때 다음 간격을 한 번 결정
    const int32 IntervalMinutes =
        FMath::RandRange(MinMinutes, MaxMinutes);

    NextArrivalSeconds =
        CurrentSeconds + IntervalMinutes * 60.0;
}

void AGuestScheduleManager::HandleRoomAssigned(AGuestCharacter* Guest)
{
    if (!IsValid(Guest) || !Clock.IsValid())
    {
        return;
    }

    const FGuid StayId = Guest->GetStayId();

    if (!StayId.IsValid())
    {
        return;
    }

    if (PendingCheckInGuest.Get() == Guest)
    {
        PendingCheckInGuest.Reset();
        PendingCheckInDeadlineSeconds = 0.0;
    }

    // 같은 투숙에 이벤트가 다시 와도 퇴실 시각을 다시 뽑지 않음
    for (const FCheckoutSchedule& Existing : CheckoutSchedules)
    {
        if (Existing.StayId == StayId)
        {
            return;
        }
    }

    const double Now = Clock->GetTotalGameSeconds();

    const int64 CheckInDayIndex =
        FMath::FloorToInt64(Now / 86400.0);

    const double NextDayStart =
        static_cast<double>(CheckInDayIndex + 1) * 86400.0;

    // 다음 날 06:00~11:00 중 분 단위로 한 번 결정
    const int32 CheckoutMinuteOfDay = FMath::RandRange(
        CheckoutStartHour * 60,
        CheckoutEndHour * 60);

    FCheckoutSchedule Entry;
    Entry.Guest = Guest;
    Entry.StayId = StayId;
    Entry.CheckoutAtSeconds =
        NextDayStart + CheckoutMinuteOfDay * 60.0;

    CheckoutSchedules.Add(Entry);
}

void AGuestScheduleManager::ProcessCheckouts(
    double CurrentSeconds)
{
    for (int32 Index = CheckoutSchedules.Num() - 1;
         Index >= 0;
         --Index)
    {
        FCheckoutSchedule& Entry = CheckoutSchedules[Index];
        AGuestCharacter* Guest = Entry.Guest.Get();

        // NPC가 사라졌거나 다른 투숙으로 재사용되었다면 이전 일정 제거
        if (!IsValid(Guest) || Guest->GetStayId() != Entry.StayId)
        {
            CheckoutSchedules.RemoveAtSwap(Index);
            continue;
        }

        const EGuestState State = Guest->GetGuestState();

        if (State == EGuestState::Inactive ||
            State == EGuestState::ExitCompleted)
        {
            CheckoutSchedules.RemoveAtSwap(Index);
            continue;
        }

        // 로비 도착 이후에는 출구로 이동
        if (State == EGuestState::ReadyToExit)
        {
            if (IsValid(ExitTarget.Get()))
            {
                Guest->TryBeginExit(ExitTarget.Get());
            }

            continue;
        }

        if (Entry.bCheckoutTimeHandled ||
            CurrentSeconds < Entry.CheckoutAtSeconds ||
            State != EGuestState::Staying)
        {
            continue;
        }

        if (!IsValid(LobbyTarget.Get()))
        {
            continue;
        }

        // 기존 Automatic / RequireRequest / Random 정책 적용
        if (Guest->HandleCheckoutTime(LobbyTarget.Get()))
        {
            Entry.bCheckoutTimeHandled = true;
        }
    }
}