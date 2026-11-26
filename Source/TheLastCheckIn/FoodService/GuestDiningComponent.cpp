#include "FoodService/GuestDiningComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoodService/Buffet/BuffetStation.h"
#include "FoodService/Data/FoodDefinition.h"
#include "FoodService/Orders/FoodOrderSubsystem.h"
#include "FoodService/Seats/DiningSeat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Guests/Anomaly/GuestAnomalyComponent.h"
#include "Guests/GuestAIController.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Party/GuestParty.h"
#include "NavigationSystem.h"
#include "Rooms/RoomActor.h"

UGuestDiningComponent::UGuestDiningComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 방문 공통 조건과 초기화
AGuestCharacter* UGuestDiningComponent::GetGuestReadyToVisit(double CurrentSeconds) const
{
    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (bVisitActive
        || !IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetGuestState() != EGuestState::Staying
        || Guest->IsActionMovementPaused())
    {
        return nullptr;
    }

    const UGuestParty* Party = Guest->GetParty();
    if (!IsValid(Party)
        || !Party->IsStaying()
        || !IsValid(Party->GetAssignedRoom())
        || !Party->GetStayId().IsValid())
    {
        return nullptr;
    }

    const UGuestAnomalyComponent* Anomaly = Guest->GetAnomalyComponent();
    if (!IsValid(Anomaly) || Anomaly->IsBusy())
    {
        return nullptr;
    }

    const double CheckoutTime = Party->GetCheckoutTime();
    if (CheckoutTime >= 0.0 && CurrentSeconds >= CheckoutTime)
    {
        return nullptr;
    }

    return Guest;
}

void UGuestDiningComponent::BeginVisit(
    EGuestMealType InMealType,
    const FGuid& StayId,
    int64 DayIndex)
{
    MealType = InMealType;
    DiningVisitId = FGuid::NewGuid();
    VisitStayId = StayId;
    VisitDay = DayIndex;

    bVisitActive = true;
    bReturning = false;
    bDinnerPrepared = InMealType == EGuestMealType::Dinner;

    WaitUntilSeconds = -1.0;
    MoveDeadlineWorldSeconds = -1.0;
    NextReturnAttemptWorldSeconds = -1.0;
}

// 조식 출발과 줄서기
bool UGuestDiningComponent::TryStartBreakfast(
    const TArray<TObjectPtr<ABuffetStation>>& Stations,
    const TArray<TObjectPtr<ADiningSeat>>& Seats,
    double CurrentSeconds)
{
    AGuestCharacter* Guest = GetGuestReadyToVisit(CurrentSeconds);
    if (!Guest)
    {
        return false;
    }

    const double WorldNow = GetWorld()->GetTimeSeconds();
    if (WorldNow < NextStartAttemptWorldSeconds)
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    const int64 DayIndex = GameTime::GetDayIndex(CurrentSeconds);

    const FGuid StayId = Party->GetStayId();

    if (LastBreakfastDay == DayIndex && LastBreakfastStayId == StayId)
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
        const int32 Index = FMath::RandRange(0, Candidates.Num() - 1);

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

        // 앉을 자리를 확보하지 못하면 이번 줄 등록을 취소
        if (!TryReserveSeat(Seats))
        {
            Station->LeaveQueue(Guest, NewVisitId);
            continue;
        }

        Buffet = Station;
        VisitId = NewVisitId;
        BeginVisit(EGuestMealType::Breakfast, StayId, DayIndex);

        MoveDeadlineWorldSeconds = WorldNow + FMath::Max(1.0f, MovementTimeoutSeconds);

        QueueTarget = QueuePoint;

        if (!Guest->BeginMoveTo(QueuePoint, EGuestState::MovingToBuffetQueue))
        {
            ClearVisit();
            continue;
        }

        LastBreakfastDay = DayIndex;
        LastBreakfastStayId = StayId;

        return true;
    }

    return false;
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

    // 앞 손님이 빠졌으면 새로 배정된 위치까지 전진
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

    // 맨 앞 위치에 도착했고 음식 자리가 비어 있을 때만 이동
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

// 석식 준비와 출발
bool UGuestDiningComponent::HasVisitedDinner(double CurrentSeconds) const
{
    const AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());
    const UGuestParty* Party = IsValid(Guest) ? Guest->GetParty() : nullptr;

    return IsValid(Party)
        && LastDinnerStayId == Party->GetStayId()
        && LastDinnerDay == GameTime::GetDayIndex(CurrentSeconds);
}

bool UGuestDiningComponent::PrepareDinner(
    const TArray<TObjectPtr<ADiningSeat>>& Seats,
    const TArray<TObjectPtr<UFoodDefinition>>& Menu,
    double CurrentSeconds)
{
    AGuestCharacter* Guest = GetGuestReadyToVisit(CurrentSeconds);
    if (!Guest)
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();
    if (Party->HasAnomalyParticipants())
    {
        return false;
    }

    const int64 DayIndex = GameTime::GetDayIndex(CurrentSeconds);
    const FGuid StayId = Party->GetStayId();

    if (HasVisitedDinner(CurrentSeconds))
    {
        return false;
    }

    TArray<UFoodDefinition*> AvailableMenu;

    for (const TObjectPtr<UFoodDefinition>& FoodRef : Menu)
    {
        UFoodDefinition* Food = FoodRef.Get();

        if (IsValid(Food) && !Food->FoodId.IsNone())
        {
            AvailableMenu.AddUnique(Food);
        }
    }

    if (AvailableMenu.IsEmpty() || !TryReserveSeat(Seats))
    {
        return false;
    }

    PendingDinnerFood = AvailableMenu[
        FMath::RandRange(0, AvailableMenu.Num() - 1)];

    BeginVisit(EGuestMealType::Dinner, StayId, DayIndex);

    return true;
}

bool UGuestDiningComponent::StartPreparedDinner()
{
    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());
    ADiningSeat* CurrentSeat = Seat.Get();

    if (!bDinnerPrepared
        || !bVisitActive
        || MealType != EGuestMealType::Dinner
        || !IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetGuestState() != EGuestState::Staying
        || !IsValid(CurrentSeat)
        || !CurrentSeat->IsReservedBy(Guest, SeatReservationId))
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    if (!IsValid(Party)
        || !Party->IsStaying()
        || Party->GetStayId() != VisitStayId
        || Party->HasAnomalyParticipants())
    {
        return false;
    }

    ATargetPoint* ApproachPoint = CurrentSeat->GetApproachPoint();

    if (!IsValid(ApproachPoint))
    {
        return false;
    }

    if (!TryMoveToSeat(Guest, ApproachPoint))
    {
        return false;
    }

    bDinnerPrepared = false;

    LastDinnerDay = VisitDay;
    LastDinnerStayId = VisitStayId;

    return true;
}

void UGuestDiningComponent::CancelDinnerDeparture()
{
    if (!bVisitActive || MealType != EGuestMealType::Dinner)
    {
        return;
    }

    if (bDinnerPrepared)
    {
        // 아직 객실을 떠나지 않았으므로 예약만 해제한다.
        ClearVisit();
        return;
    }

    BeginReturnToRoom();
}

// 방문 상태 진행
void UGuestDiningComponent::AdvanceDining(double CurrentSeconds, bool bBreakfastOpen)
{
    if (!bVisitActive || bDinnerPrepared)
    {
        return;
    }

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (!IsValid(Guest) || Guest->IsActorBeingDestroyed())
    {
        ClearVisit();
        return;
    }

    UGuestParty* Party = Guest->GetParty();

    if (!IsValid(Party)
        || Party->GetStayId() != VisitStayId
        || !Party->IsStaying())
    {
        // 방문이 취소되어도 앉아 있던 손님은 먼저 의자에서 나온다.
        if (!PrepareForMovement())
        {
            return;
        }

        if (Guest->GetGuestState() == EGuestState::WaitingForCheckoutRequest)
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

    const bool bInQueue =
        State == EGuestState::MovingToBuffetQueue
        || State == EGuestState::WaitingInBuffetQueue;

    const bool bAtBuffet =
        State == EGuestState::MovingToBuffet
        || State == EGuestState::WaitingAtBuffet;

    const bool bAtSeat =
        State == EGuestState::MovingToDiningSeat
        || State == EGuestState::ReadyToSit
        || State == EGuestState::Seated
        || State == EGuestState::LeavingDiningSeat;

    if (bReturning)
    {
        if (State == EGuestState::Staying)
        {
            ClearVisit();
            return;
        }

        if (!bInQueue
            && !bAtBuffet
            && !bAtSeat
            && State != EGuestState::ReturningFromBuffet)
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

    if (!bInQueue && !bAtBuffet && !bAtSeat)
    {
        ClearVisit();
        return;
    }

    const double CheckoutTime = Party->GetCheckoutTime();

    const bool bCheckoutDue =
        CheckoutTime >= 0.0
        && CurrentSeconds >= CheckoutTime;

    const bool bDayChanged = GameTime::GetDayIndex(CurrentSeconds) != VisitDay;

    ADiningSeat* CurrentSeat = Seat.Get();

    if (bCheckoutDue
        || bDayChanged
        || !IsValid(CurrentSeat)
        || CurrentSeat->IsActorBeingDestroyed()
        || !CurrentSeat->IsReservedBy(Guest, SeatReservationId))
    {
        BeginReturnToRoom();
        return;
    }

    if (State == EGuestState::MovingToDiningSeat)
    {
        if (WorldNow >= MoveDeadlineWorldSeconds)
        {
            BeginReturnToRoom();
        }

        return;
    }

    if (State == EGuestState::ReadyToSit)
    {
        ReleaseBuffetReservation();

        if (!TrySitDown(CurrentSeconds))
        {
            BeginReturnToRoom();
            return;
        }

        // 석식 주문은 실제 착석이 끝난 뒤 생성
        if (MealType == EGuestMealType::Dinner
            && !CreateDinnerOrder(CurrentSeconds))
        {
            BeginReturnToRoom();
        }

        return;
    }

    if (State == EGuestState::Seated)
    {
        if (CurrentSeconds >= WaitUntilSeconds)
        {
            if (MealType == EGuestMealType::Dinner)
            {
                // 음식 제공 전 단계이므로 주문을 완료로 처리하지 않음
                CancelDinnerOrder(EFoodOrderCancelReason::PreviewTimeEnded);
            }

            BeginReturnToRoom();
        }

        return;
    }

    if (State == EGuestState::LeavingDiningSeat)
    {
        BeginReturnToRoom();
        return;
    }

    // 아래의 뷔페 처리에는 조식 손님만 들어간다.
    if (MealType != EGuestMealType::Breakfast)
    {
        BeginReturnToRoom();
        return;
    }

    ABuffetStation* Station = Buffet.Get();

    // 이미 의자로 이동했거나 착석한 손님은 영업 종료 후에도 식사를 마친다.
    if (!bBreakfastOpen
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
            BeginReturnToRoom();
        }

        return;
    }

    if (WaitUntilSeconds < 0.0)
    {
        WaitUntilSeconds =
            CurrentSeconds
            + GameTime::MinutesToSeconds(FMath::Max(0.1f, BuffetWaitGameMinutes));
    }

    if (CurrentSeconds >= WaitUntilSeconds)
    {
        BeginMoveToSeat();
    }
}

// 좌석 예약과 이동
bool UGuestDiningComponent::TryReserveSeat(const TArray<TObjectPtr<ADiningSeat>>& Seats)
{
    TArray<ADiningSeat*> Candidates;

    for (const TObjectPtr<ADiningSeat>& SeatRef : Seats)
    {
        ADiningSeat* Candidate = SeatRef.Get();

        if (IsValid(Candidate)
            && !Candidate->IsActorBeingDestroyed()
            && Candidate->GetWorld() == GetWorld())
        {
            Candidates.AddUnique(Candidate);
        }
    }

    const FGuid NewReservationId = FGuid::NewGuid();

    while (!Candidates.IsEmpty())
    {
        const int32 Index = FMath::RandRange(0, Candidates.Num() - 1);

        ADiningSeat* Candidate = Candidates[Index];
        Candidates.RemoveAtSwap(Index);

        if (!Candidate->TryReserve(GetOwner(), NewReservationId))
        {
            continue;
        }

        Seat = Candidate;
        SeatReservationId = NewReservationId;
        return true;
    }

    return false;
}

void UGuestDiningComponent::BeginMoveToSeat()
{
    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());
    ADiningSeat* CurrentSeat = Seat.Get();

    if (!IsValid(Guest)
        || !IsValid(CurrentSeat)
        || !CurrentSeat->IsReservedBy(Guest, SeatReservationId))
    {
        BeginReturnToRoom();
        return;
    }

    if (!TryMoveToSeat(Guest, CurrentSeat->GetApproachPoint()))
    {
        BeginReturnToRoom();
    }

    // 음식 자리 예약은 의자 앞에 도착했을 때 해제한다.
}

bool UGuestDiningComponent::TryMoveToSeat(
    AGuestCharacter* Guest,
    ATargetPoint* ApproachPoint)
{
    Guest->CancelAction();

    if (!IsValid(ApproachPoint)
        || !Guest->BeginMoveTo(ApproachPoint, EGuestState::MovingToDiningSeat))
    {
        return false;
    }

    WaitUntilSeconds = -1.0;
    MoveDeadlineWorldSeconds =
        GetWorld()->GetTimeSeconds()
        + FMath::Max(1.0f, MovementTimeoutSeconds);
    return true;
}

bool UGuestDiningComponent::TrySitDown(double CurrentSeconds)
{
    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());
    ADiningSeat* CurrentSeat = Seat.Get();

    if (!IsValid(Guest)
        || !IsValid(CurrentSeat)
        || !CurrentSeat->IsReservedBy(Guest, SeatReservationId))
    {
        return false;
    }

    UCharacterMovementComponent* Movement = Guest->GetCharacterMovement();

    AGuestAIController* Controller = Cast<AGuestAIController>(Guest->GetController());

    if (!IsValid(Movement) || !IsValid(Controller))
    {
        return false;
    }

    const FTransform SeatTransform = CurrentSeat->GetSeatTransform();
    const FVector SeatLocation = SeatTransform.GetLocation();

    const FRotator SeatRotation(0.0f, SeatTransform.Rotator().Yaw,0.0f);

    // 앉을 의자를 제외한 벽, 가구, 다른 손님과의 충돌을 검사한다.
    if (!CanPlaceGuest(SeatLocation, CurrentSeat))
    {
        return false;
    }

    const float HalfHeight = Guest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

    SeatExitFloorLocation =
        Guest->GetActorLocation()
        - FVector(0.0, 0.0, HalfHeight);

    SavedMovementMode = static_cast<uint8>(Movement->MovementMode);
    SavedCustomMovementMode = Movement->CustomMovementMode;

    Guest->CancelAction();
    Controller->StopGuestAI();
    Guest->MoveTarget = nullptr;

    Movement->StopMovementImmediately();
    Movement->DisableMovement();

    bPlacedAtSeat = true;

    // 접근 이동이 끝난 뒤 착석 위치에 배치한다.
    // 애니메이션은 이후 이 지점에 연결
    if (!Guest->SetActorLocationAndRotation(
            SeatLocation,
            SeatRotation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics))
    {
        RestoreSeatMovement();
        return false;
    }

    Controller->SetControlRotation(SeatRotation);
    Controller->UseWaitingAvoidance();

    Guest->GuestState = EGuestState::Seated;

    const float StayMinutes =
        MealType == EGuestMealType::Dinner ? DinnerStayGameMinutes : SeatStayGameMinutes;

    WaitUntilSeconds =
        CurrentSeconds + GameTime::MinutesToSeconds(FMath::Max(0.1f, StayMinutes));

    return true;
}

bool UGuestDiningComponent::PrepareForMovement()
{
    return !bPlacedAtSeat || TryLeaveSeat();
}

bool UGuestDiningComponent::TryLeaveSeat()
{
    if (!bPlacedAtSeat)
    {
        return true;
    }

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (!IsValid(Guest))
    {
        return false;
    }

    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    FNavLocation ExitFloor;

    if (!Navigation
        || !Navigation->ProjectPointToNavigation(
            SeatExitFloorLocation,
            ExitFloor,
            FVector(30.0, 30.0, 200.0),
            &Guest->GetNavAgentPropertiesRef()))
    {
        return false;
    }

    // 원래 접근 위치에서 크게 벗어난 곳으로 나오지 않는다.
    if (FVector::DistSquared2D(
            SeatExitFloorLocation,
            ExitFloor.Location) > FMath::Square(10.0))
    {
        return false;
    }

    const float HalfHeight = Guest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

    const FVector ExitCenter =
        ExitFloor.Location
        + FVector(0.0, 0.0, HalfHeight + 5.0f);

    // 의자 앞을 다른 손님이 지나가고 있으면 기다린다.
    if (!CanPlaceGuest(ExitCenter, nullptr))
    {
        return false;
    }

    if (!Guest->SetActorLocation(
            ExitCenter,
            false,
            nullptr,
            ETeleportType::TeleportPhysics))
    {
        return false;
    }

    RestoreSeatMovement();
    ReleaseSeatReservation();
    return true;
}

bool UGuestDiningComponent::CanPlaceGuest(const FVector& CapsuleCenter, const AActor* IgnoredActor) const
{
    const AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    UWorld* World = GetWorld();

    if (!IsValid(Guest) || !World || CapsuleCenter.ContainsNaN())
    {
        return false;
    }

    const UCapsuleComponent* Capsule = Guest->GetCapsuleComponent();

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Guest);

    if (IsValid(IgnoredActor))
    {
        Params.AddIgnoredActor(IgnoredActor);
    }

    if (World->OverlapBlockingTestByProfile(
            CapsuleCenter,
            FQuat::Identity,
            Capsule->GetCollisionProfileName(),
            FCollisionShape::MakeCapsule(Radius, HalfHeight),
            Params))
    {
        return false;
    }

    // 다른 손님이 도착할 예정인 자리도 비워 둔다.
    for (TActorIterator<AGuestCharacter> It(World); It; ++It)
    {
        const AGuestCharacter* Other = *It;

        if (!IsValid(Other)
            || Other == Guest
            || !Other->bHasReservedMoveLocation)
        {
            continue;
        }

        const UCapsuleComponent* OtherCapsule = Other->GetCapsuleComponent();

        const float OtherRadius = OtherCapsule->GetScaledCapsuleRadius();

        const float OtherHalfHeight = OtherCapsule->GetScaledCapsuleHalfHeight();

        const FVector OtherCenter =
            Other->ReservedMoveLocation
            + FVector(0.0, 0.0, OtherHalfHeight + 5.0f);

        const bool bHeightOverlaps =
            FMath::Abs(CapsuleCenter.Z - OtherCenter.Z)
            < HalfHeight + OtherHalfHeight;

        const float RequiredDistance =
            Radius + OtherRadius
            + AGuestCharacter::MoveAcceptanceRadius * 2.0f
            + 10.0f;

        if (bHeightOverlaps
            && FVector::DistSquared2D(CapsuleCenter, OtherCenter)
                < FMath::Square(RequiredDistance))
        {
            return false;
        }
    }

    return true;
}

void UGuestDiningComponent::RestoreSeatMovement()
{
    if (!bPlacedAtSeat)
    {
        return;
    }

    if (AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement =
            Guest->GetCharacterMovement())
        {
            Movement->SetMovementMode(
                static_cast<EMovementMode>(SavedMovementMode),
                SavedCustomMovementMode);
        }
    }

    bPlacedAtSeat = false;
}

void UGuestDiningComponent::ReleaseSeatReservation()
{
    if (ADiningSeat* CurrentSeat = Seat.Get())
    {
        CurrentSeat->Release(GetOwner(), SeatReservationId);
    }

    Seat.Reset();
    SeatReservationId.Invalidate();
}

// 석식 주문
bool UGuestDiningComponent::CreateDinnerOrder(double CurrentSeconds)
{
    if (DinnerOrderId.IsValid())
    {
        return true;
    }

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    UFoodOrderSubsystem* Orders = GetWorld()->GetSubsystem<UFoodOrderSubsystem>();

    if (!IsValid(Guest)
        || !IsValid(Orders)
        || !IsValid(PendingDinnerFood.Get()))
    {
        return false;
    }

    DinnerOrderId = Orders->CreateRestaurantOrder(
        Guest,
        VisitStayId,
        DiningVisitId,
        PendingDinnerFood.Get(),
        CurrentSeconds);

    return DinnerOrderId.IsValid();
}

bool UGuestDiningComponent::CanTakeDinnerOrder() const
{
    const AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    if (!bVisitActive
        || bReturning
        || !bPlacedAtSeat
        || MealType != EGuestMealType::Dinner
        || !DinnerOrderId.IsValid()
        || !IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetGuestState() != EGuestState::Seated)
    {
        return false;
    }

    const UGuestParty* Party = Guest->GetParty();
    const ADiningSeat* CurrentSeat = Seat.Get();

    if (!IsValid(Party)
        || !Party->IsStaying()
        || Party->GetStayId() != VisitStayId
        || !IsValid(CurrentSeat)
        || CurrentSeat->IsActorBeingDestroyed()
        || !CurrentSeat->IsReservedBy(Guest, SeatReservationId))
    {
        return false;
    }

    const UGameClockSubsystem* Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>();

    if (!IsValid(Clock))
    {
        return false;
    }

    const double CurrentSeconds = Clock->GetTotalGameSeconds();
    const double CheckoutTime = Party->GetCheckoutTime();

    if (CurrentSeconds >= WaitUntilSeconds
        || (CheckoutTime >= 0.0 && CurrentSeconds >= CheckoutTime))
    {
        return false;
    }

    FFoodOrderRecord Order;

    return GetDinnerOrder(Order)
        && Order.StayId == VisitStayId
        && Order.VisitId == DiningVisitId
        && Order.State == EFoodOrderState::PendingAcceptance;
}

bool UGuestDiningComponent::TakeDinnerOrder()
{
    if (!CanTakeDinnerOrder())
    {
        return false;
    }

    UFoodOrderSubsystem* Orders =
        GetWorld()->GetSubsystem<UFoodOrderSubsystem>();

    return IsValid(Orders)
        && Orders->TryAcceptOrder(
            DinnerOrderId,
            Cast<AGuestCharacter>(GetOwner()),
            VisitStayId,
            DiningVisitId);
}

bool UGuestDiningComponent::GetDinnerOrder(FFoodOrderRecord& OutOrder) const
{
    OutOrder = FFoodOrderRecord();

    UWorld* World = GetWorld();

    if (!World || !DinnerOrderId.IsValid())
    {
        return false;
    }

    const UFoodOrderSubsystem* Orders =
        World->GetSubsystem<UFoodOrderSubsystem>();

    return IsValid(Orders)
        && Orders->GetOrder(DinnerOrderId, OutOrder);
}

void UGuestDiningComponent::CancelDinnerOrder(EFoodOrderCancelReason Reason)
{
    if (!DinnerOrderId.IsValid())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (UFoodOrderSubsystem* Orders = World->GetSubsystem<UFoodOrderSubsystem>())
        {
            Orders->CancelOrder(DinnerOrderId, Reason);
        }
    }

    DinnerOrderId.Invalidate();
}

// 객실 복귀와 방문 정리
void UGuestDiningComponent::BeginReturnToRoom()
{
    CancelDinnerOrder(EFoodOrderCancelReason::VisitEnded);

    bReturning = true;

    const double WorldNow = GetWorld()->GetTimeSeconds();

    // 이동을 시작하지 못하면 월드 시간 5초 뒤 다시 시도
    NextReturnAttemptWorldSeconds = WorldNow + 5.0;

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    UGuestParty* Party = IsValid(Guest) ? Guest->GetParty() : nullptr;

    if (!IsValid(Guest)
        || !IsValid(Party)
        || Party->GetStayId() != VisitStayId
        || !Party->IsStaying())
    {
        return;
    }

    if (bPlacedAtSeat)
    {
        Guest->GuestState = EGuestState::LeavingDiningSeat;
    }

    // 앉아 있다면 이동을 시작하기 전에 의자 앞으로 나온다.
    if (!PrepareForMovement())
    {
        return;
    }

    ARoomActor* Room = Party->GetAssignedRoom();

    AActor* Target = IsValid(Room) ? Room->GetArrivalTarget() : nullptr;

    if (!IsValid(Target))
    {
        return;
    }

    Guest->CancelAction();

    if (!Guest->BeginMoveTo(Target, EGuestState::ReturningFromBuffet))
    {
        return;
    }

    ReleaseBuffetReservation();
    ReleaseSeatReservation();

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
    CancelDinnerOrder(EFoodOrderCancelReason::VisitEnded);

    RestoreSeatMovement();
    ReleaseSeatReservation();
    ReleaseBuffetReservation();

    MealType = EGuestMealType::None;
    PendingDinnerFood = nullptr;

    DiningVisitId.Invalidate();
    VisitStayId.Invalidate();
    VisitDay = -1;

    bVisitActive = false;
    bReturning = false;
    bDinnerPrepared = false;

    WaitUntilSeconds = -1.0;
    MoveDeadlineWorldSeconds = -1.0;
    NextReturnAttemptWorldSeconds = -1.0;

    SeatExitFloorLocation = FVector::ZeroVector;

    // 당일 재방문을 막기 위해 조식, 석식 방문 기록은 유지
}

void UGuestDiningComponent::ResetForPool()
{
    ClearVisit();

    LastBreakfastStayId.Invalidate();
    LastBreakfastDay = -1;

    LastDinnerStayId.Invalidate();
    LastDinnerDay = -1;

    NextStartAttemptWorldSeconds = 0.0;
}

void UGuestDiningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetForPool();

    Super::EndPlay(EndPlayReason);
}
