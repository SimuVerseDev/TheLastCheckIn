#include "FoodService/GuestDiningComponent.h"

#include "FoodService/Buffet/BuffetStation.h"
#include "Core/Time/GameTimeMath.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/Party/GuestParty.h"
#include "Guests/Anomaly/GuestAnomalyComponent.h"
#include "Rooms/RoomActor.h"
#include "Engine/World.h"

UGuestDiningComponent::UGuestDiningComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UGuestDiningComponent::TryStartBreakfast(
    const TArray<TObjectPtr<ABuffetStation>>& Stations,
    double CurrentSeconds)
{
    AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    if (bVisitActive
        || !IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetGuestState() != EGuestState::Staying
        || Guest->IsActionMovementPaused())
    {
        return false;
    }

    const double WorldNow = GetWorld()->GetTimeSeconds();

    if (WorldNow < NextStartAttemptWorldSeconds)
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    if (!IsValid(Party)
        || !Party->IsStaying()
        || !IsValid(Party->GetAssignedRoom())
        || !Party->GetStayId().IsValid())
    {
        return false;
    }

    const UGuestAnomalyComponent* Anomaly =
        Guest->GetAnomalyComponent();

    if (!IsValid(Anomaly) || Anomaly->IsBusy())
    {
        return false;
    }

    const double CheckoutTime = Party->GetCheckoutTime();

    if (CheckoutTime >= 0.0 && CurrentSeconds >= CheckoutTime)
    {
        return false;
    }

    const int64 DayIndex =
        GameTime::GetDayIndex(CurrentSeconds);

    const FGuid StayId = Party->GetStayId();

    if (LastVisitDay == DayIndex
        && LastVisitStayId == StayId)
    {
        return false;
    }

    TArray<ABuffetStation*> Candidates;

    for (const TObjectPtr<ABuffetStation>& StationRef : Stations)
    {
        ABuffetStation* Station = StationRef.Get();

        if (IsValid(Station)
            && !Station->IsActorBeingDestroyed()
            && Station->GetWorld() == GetWorld()
            && Station->HasFood())
        {
            Candidates.AddUnique(Station);
        }
    }

    if (Candidates.IsEmpty())
    {
        return false;
    }

    // 줄 등록이나 출발이 실패하면 잠시 후 다시 시도한다.
    NextStartAttemptWorldSeconds = WorldNow + 5.0;

    while (!Candidates.IsEmpty())
    {
        const int32 Index =
            FMath::RandRange(0, Candidates.Num() - 1);

        ABuffetStation* Station = Candidates[Index];
        Candidates.RemoveAtSwap(Index);

        if (!IsValid(Station) || Station->IsActorBeingDestroyed())
        {
            continue;
        }

        const FGuid NewVisitId = FGuid::NewGuid();

        if (!Station->TryJoinQueue(Guest, NewVisitId))
        {
            continue;
        }

        AActor* QueuePoint = Station->GetQueuePoint(Guest, NewVisitId);
        if (!IsValid(QueuePoint))
        {
            Station->LeaveQueue(Guest, NewVisitId);
            continue;
        }

        Buffet = Station;
        VisitId = NewVisitId;
        VisitStayId = StayId;

        bVisitActive = true;
        bReturning = false;

        WaitUntilSeconds = -1.0;

        MoveDeadlineWorldSeconds =
            WorldNow + FMath::Max(1.0f, MovementTimeoutSeconds);

        QueueTarget = QueuePoint;

        if (!Guest->BeginMoveTo(
                QueuePoint,
                EGuestState::MovingToBuffetQueue))
        {
            ClearVisit();
            continue;
        }

        LastVisitDay = DayIndex;
        LastVisitStayId = StayId;

        return true;
    }

    return false;
}

void UGuestDiningComponent::AdvanceBreakfast(
    double CurrentSeconds,
    bool bBreakfastOpen)
{
    if (!bVisitActive)
    {
        return;
    }

    AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    UGuestParty* Party =
        IsValid(Guest) ? Guest->GetParty() : nullptr;

    if (!IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || !IsValid(Party)
        || Party->GetStayId() != VisitStayId
        || !Party->IsStaying())
    {
        // 퇴실 요청 대기로 바뀐 손님은 이전 식당 이동을 중단한다.
        if (IsValid(Guest)
            && !Guest->IsActorBeingDestroyed()
            && Guest->GetGuestState()
                == EGuestState::WaitingForCheckoutRequest)
        {
            if (AGuestAIController* Controller =
                Cast<AGuestAIController>(Guest->GetController()))
            {
                Controller->StopGuestAI();
                Controller->UseWaitingAvoidance();
            }

            Guest->MoveTarget = nullptr;
        }

        ClearVisit();
        return;
    }

    const EGuestState State = Guest->GetGuestState();
    const double WorldNow = GetWorld()->GetTimeSeconds();
    const bool bInQueue = State == EGuestState::MovingToBuffetQueue
        || State == EGuestState::WaitingInBuffetQueue;
    const bool bAtBuffet = State == EGuestState::MovingToBuffet
        || State == EGuestState::WaitingAtBuffet;

    if (bReturning)
    {
        if (State == EGuestState::Staying)
        {
            // 실제 객실 도착 후 방문을 종료한다.
            ClearVisit();
            return;
        }

        if (!bInQueue && !bAtBuffet && State != EGuestState::ReturningFromBuffet)
        {
            ClearVisit();
            return;
        }

        if (WorldNow >= NextReturnAttemptWorldSeconds)
        {
            BeginReturnToRoom();
        }

        return;
    }

    if (!bInQueue && !bAtBuffet)
    {
        ClearVisit();
        return;
    }

    ABuffetStation* Station = Buffet.Get();

    const double CheckoutTime = Party->GetCheckoutTime();

    const bool bCheckoutDue =
        CheckoutTime >= 0.0 && CurrentSeconds >= CheckoutTime;

    const bool bDayChanged =
        GameTime::GetDayIndex(CurrentSeconds) != LastVisitDay;

    if (!bBreakfastOpen
        || bDayChanged
        || bCheckoutDue
        || !IsValid(Station)
        || Station->IsActorBeingDestroyed()
        || !Station->HasFood())
    {
        BeginReturnToRoom();
        return;
    }

    if (bInQueue)
    {
        UpdateQueue(WorldNow);
        return;
    }

    if (State == EGuestState::MovingToBuffet)
    {
        if (WorldNow >= MoveDeadlineWorldSeconds)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Breakfast: buffet movement timed out for %s."),
                *GetNameSafe(Guest));

            BeginReturnToRoom();
        }

        return;
    }

    if (WaitUntilSeconds < 0.0)
    {
        // 식사 기능을 붙이기 전까지는 대기 후 복귀한다.
        WaitUntilSeconds =
            CurrentSeconds
            + GameTime::MinutesToSeconds(
                FMath::Max(0.1f, BuffetWaitGameMinutes));
    }

    if (CurrentSeconds >= WaitUntilSeconds)
    {
        BeginReturnToRoom();
    }
}

void UGuestDiningComponent::UpdateQueue(double WorldNow)
{
    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());
    ABuffetStation* Station = Buffet.Get();
    if (!IsValid(Guest) || !IsValid(Station))
    {
        BeginReturnToRoom();
        return;
    }

    AActor* DesiredPoint = Station->GetQueuePoint(Guest, VisitId);
    if (!IsValid(DesiredPoint))
    {
        BeginReturnToRoom();
        return;
    }

    // 앞 손님이 빠졌으면 새로 배정된 위치까지 전진한다.
    if (QueueTarget.Get() != DesiredPoint)
    {
        if (!Guest->BeginMoveTo(DesiredPoint, EGuestState::MovingToBuffetQueue))
        {
            BeginReturnToRoom();
            return;
        }

        QueueTarget = DesiredPoint;
        MoveDeadlineWorldSeconds = WorldNow + FMath::Max(1.0f, MovementTimeoutSeconds);
        return;
    }

    if (Guest->GetGuestState() == EGuestState::MovingToBuffetQueue)
    {
        if (WorldNow >= MoveDeadlineWorldSeconds)
        {
            BeginReturnToRoom();
        }
        return;
    }

    // 맨 앞 위치에 도착했고 음식 자리가 비어 있을 때만 이동한다.
    if (Guest->GetGuestState() != EGuestState::WaitingInBuffetQueue
        || !Station->TryReserveServingPoint(Guest, VisitId))
    {
        return;
    }

    if (!Guest->BeginMoveTo(Station, EGuestState::MovingToBuffet))
    {
        BeginReturnToRoom();
        return;
    }

    QueueTarget.Reset();
    WaitUntilSeconds = -1.0;
    MoveDeadlineWorldSeconds = WorldNow + FMath::Max(1.0f, MovementTimeoutSeconds);
}

void UGuestDiningComponent::BeginReturnToRoom()
{
    bReturning = true;

    const double WorldNow = GetWorld()->GetTimeSeconds();

    // 복귀를 시작하지 못하면 월드 시간 5초 뒤 재시도한다.
    NextReturnAttemptWorldSeconds = WorldNow + 5.0;

    AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    UGuestParty* Party =
        IsValid(Guest) ? Guest->GetParty() : nullptr;

    if (!IsValid(Guest)
        || !IsValid(Party)
        || Party->GetStayId() != VisitStayId
        || !Party->IsStaying())
    {
        return;
    }

    ARoomActor* Room = Party->GetAssignedRoom();

    AActor* Target = IsValid(Room)
        ? Room->GetArrivalTarget()
        : nullptr;

    if (!IsValid(Target))
    {
        return;
    }

    Guest->CancelAction();

    if (!Guest->BeginMoveTo(
            Target,
            EGuestState::ReturningFromBuffet))
    {
        return;
    }

    ReleaseBuffetReservation();

    NextReturnAttemptWorldSeconds =
        WorldNow + FMath::Max(1.0f, MovementTimeoutSeconds);
}

void UGuestDiningComponent::ReleaseBuffetReservation()
{
    if (ABuffetStation* Station = Buffet.Get())
    {
        Station->LeaveQueue(GetOwner(), VisitId);
    }

    QueueTarget.Reset();
    Buffet.Reset();
    VisitId.Invalidate();
}

void UGuestDiningComponent::ClearVisit()
{
    ReleaseBuffetReservation();

    VisitStayId.Invalidate();

    bVisitActive = false;
    bReturning = false;

    WaitUntilSeconds = -1.0;
    MoveDeadlineWorldSeconds = -1.0;
    NextReturnAttemptWorldSeconds = -1.0;

    // 당일 재방문을 막기 위해 LastVisitDay는 유지한다.
}

void UGuestDiningComponent::ResetForPool()
{
    ClearVisit();

    LastVisitStayId.Invalidate();
    LastVisitDay = -1;
    NextStartAttemptWorldSeconds = 0.0;
}

void UGuestDiningComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    ResetForPool();

    Super::EndPlay(EndPlayReason);
}
