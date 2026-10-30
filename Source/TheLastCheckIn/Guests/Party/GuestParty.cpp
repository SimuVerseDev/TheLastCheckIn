#include "Guests/Party/GuestParty.h"

#include "Guests/GuestCharacter.h"
#include "UObject/UObjectGlobals.h"
#include "Guests/GuestAIController.h"
#include "Rooms/RoomActor.h"
#include "Engine/World.h"

UWorld* UGuestParty::GetWorld() const
{
    const AActor* Owner = GetTypedOuter<AActor>();
    return IsValid(Owner) ? Owner->GetWorld() : nullptr;
}

UGuestParty* UGuestParty::Create(UObject* InOuter, const TArray<AGuestCharacter*>& InMembers)
{
    if (!IsValid(InOuter) ||
        InMembers.Num() < 1 ||
        InMembers.Num() > 4)
    {
        return nullptr;
    }

    TSet<AGuestCharacter*> UniqueMembers;

    // 전원 검사에 성공한 뒤에만 일행을 생성
    for (AGuestCharacter* Member : InMembers)
    {
        if (!IsValid(Member) ||
            UniqueMembers.Contains(Member) ||
            Member->GetParty() != nullptr)
        {
            return nullptr;
        }

        const EGuestState State = Member->GetGuestState();

        if (State != EGuestState::MovingToCounter &&
            State != EGuestState::WaitingForCheckIn)
        {
            return nullptr;
        }

        UniqueMembers.Add(Member);
    }
    
    // 대표 손님의 데이터에서 일행의 퇴실 정책을 읽음
    const FGuestTypeRow* GuestData =
        InMembers[0]->GuestTypeRow.GetRow<FGuestTypeRow>(
            TEXT("GuestParty.Create"));

    if (!GuestData || GuestData->BehaviorRow.IsNull())
    {
        return nullptr;
    }

    const FGuestBehaviorRow* BehaviorData =
        GuestData->BehaviorRow.GetRow<FGuestBehaviorRow>(
            TEXT("GuestParty.Create.Behavior"));

    if (!BehaviorData)
    {
        return nullptr;
    }

    UGuestParty* NewParty = NewObject<UGuestParty>(InOuter);
    NewParty->PartyId = FGuid::NewGuid();
    
    NewParty->CheckoutPolicy = BehaviorData->CheckoutPolicy;
    NewParty->StayBehindChance = FMath::Clamp(
        BehaviorData->StayBehindChance, 0.0f, 1.0f);

    for (AGuestCharacter* Member : InMembers)
    {
        NewParty->Members.Add(
            TWeakObjectPtr<AGuestCharacter>(Member));

        // 모든 구성원이 동일한 일행 객체를 참조
        Member->Party = NewParty;
    }

    UE_LOG(LogTemp, Log,
        TEXT("Party created: %s, members: %d"),
        *NewParty->PartyId.ToString(),
        NewParty->Members.Num());

    return NewParty;
}

AGuestCharacter* UGuestParty::GetLeader() const
{
    if (Members.IsEmpty())
    {
        return nullptr;
    }

    AGuestCharacter* Leader = Members[0].Get();

    // 대표 NPC가 다른 일행으로 재사용된 경우 제외
    if (!IsValid(Leader) || Leader->GetParty() != this)
    {
        return nullptr;
    }

    return Leader;
}

TArray<AGuestCharacter*> UGuestParty::GetMembers() const
{
    TArray<AGuestCharacter*> Result;

    for (const TWeakObjectPtr<AGuestCharacter>& WeakMember : Members)
    {
        AGuestCharacter* Member = WeakMember.Get();

        if (IsValid(Member) && Member->GetParty() == this)
        {
            Result.Add(Member);
        }
    }

    return Result;
}

void UGuestParty::ConfigureVisit(
    double DeadlineSeconds,
    AActor* InLobbyTarget,
    AActor* InExitTarget)
{
    CheckInDeadlineSeconds = DeadlineSeconds;
    LobbyTarget = InLobbyTarget;
    ExitTarget = InExitTarget;

    StopMovementMonitoring();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(MovementWatchdogTimer, this,
            &UGuestParty::CheckMovementProgress, 1.0f, true);
    }
}

void UGuestParty::ConfigureMovementRecovery(float TimeoutSeconds, int32 MaxRetries)
{
    MovementTimeoutSeconds = FMath::Max(1.0f, TimeoutSeconds);
    MaxMovementRetries = FMath::Max(0, MaxRetries);
}

void UGuestParty::StopMovementMonitoring()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MovementWatchdogTimer);
    }
    MovementAttemptStartedAt = -1.0;
    MovementRetryCount = 0;
}

void UGuestParty::CheckMovementProgress()
{
    UWorld* World = GetWorld();
    if (!World || IsFinished())
    {
        StopMovementMonitoring();
        return;
    }

    EGuestState MovingState;
    EGuestState ArrivedState;
    AActor* Target = nullptr;
    switch (State)
    {
    case EGuestPartyState::CheckingIn:
        MovingState = EGuestState::MovingToCounter;
        ArrivedState = EGuestState::WaitingForCheckIn;
        if (GetMembers().Num() != GetSize())
        {
            CancelVisitAfterMovementFailure();
            return;
        }
        break;
    case EGuestPartyState::MovingToRoom:
        MovingState = EGuestState::MovingToRoom;
        ArrivedState = EGuestState::Staying;
        Target = IsValid(AssignedRoom.Get()) ? AssignedRoom->GetArrivalTarget() : nullptr;
        break;
    case EGuestPartyState::MovingToLobby:
        MovingState = EGuestState::MovingToLobby;
        ArrivedState = EGuestState::ReadyToExit;
        Target = LobbyTarget.Get();
        break;
    case EGuestPartyState::MovingToExit:
        MovingState = EGuestState::MovingToExit;
        ArrivedState = EGuestState::ExitCompleted;
        Target = ExitTarget.Get();
        break;
    default:
        // 정상 투숙/접수 대기/퇴실 요청 대기는 이동 실패로 취급하지 않음.
        MovementAttemptStartedAt = -1.0;
        MovementRetryCount = 0;
        return;
    }

    TArray<AGuestCharacter*> PendingMembers;
    for (AGuestCharacter* Member : GetMembers())
    {
        if (Member->GetGuestState() != ArrivedState)
        {
            AActor* MemberTarget = State == EGuestPartyState::CheckingIn
                ? Member->GetInitialMoveTarget() : Target;
            if (!IsValid(MemberTarget))
            {
                CancelVisitAfterMovementFailure();
                return;
            }
            PendingMembers.Add(Member);
        }
    }

    if (PendingMembers.IsEmpty())
    {
        MovementAttemptStartedAt = -1.0;
        MovementRetryCount = 0;
        return;
    }

    const double Now = World->GetTimeSeconds();
    if (MonitoredState != State || MovementAttemptStartedAt < 0.0)
    {
        MonitoredState = State;
        MovementAttemptStartedAt = Now;
        MovementRetryCount = 0;
    }
    if (Now - MovementAttemptStartedAt < MovementTimeoutSeconds)
    {
        return;
    }

    if (MovementRetryCount >= MaxMovementRetries)
    {
        CancelVisitAfterMovementFailure();
        return;
    }

    ++MovementRetryCount;
    MovementAttemptStartedAt = Now;
    UE_LOG(LogTemp, Warning, TEXT("GuestParty %s: Movement timeout in state %d; retry %d/%d for %d guests."),
        *PartyId.ToString(), static_cast<int32>(State), MovementRetryCount, MaxMovementRetries, PendingMembers.Num());
    for (AGuestCharacter* Member : PendingMembers)
    {
        // 이미 이동 중인 손님도 재시작. 도착한 손님은 그대로 기다림.
        Member->BeginMoveTo(State == EGuestPartyState::CheckingIn
            ? Member->GetInitialMoveTarget() : Target, MovingState);
    }
}

void UGuestParty::CancelVisitAfterMovementFailure()
{
    StopMovementMonitoring();

    if (IsValid(AssignedRoom.Get()))
    {
        bool bRoomReleased = true;
        if (AssignedRoom->GetRoomState() == ERoomState::Assigned)
        {
            bRoomReleased = AssignedRoom->CancelReservation(StayId);
        }
        else if (AssignedRoom->GetRoomState() == ERoomState::Occupied)
        {
            bRoomReleased = AssignedRoom->TryEndStay(StayId);
        }
        if (!bRoomReleased)
        {
            UE_LOG(LogTemp, Error, TEXT("GuestParty %s: Room stay ID mismatch; room left untouched."), *PartyId.ToString());
        }
    }

    AssignedRoom = nullptr;
    StayId.Invalidate();
    State = EGuestPartyState::Finished;
    // 풀 반환 과정에서 Party가 해제되므로 먼저 구성원 목록을 복사.
    const TArray<AGuestCharacter*> CurrentMembers = GetMembers();
    for (AGuestCharacter* Member : CurrentMembers)
    {
        Member->AbortVisitAndReturnToPool();
    }
}

bool UGuestParty::AllMembersInState(EGuestState RequiredState) const
{
    const TArray<AGuestCharacter*> CurrentMembers = GetMembers();

    if (CurrentMembers.IsEmpty())
    {
        return false;
    }

    for (AGuestCharacter* Member : CurrentMembers)
    {
        if (Member->GetGuestState() != RequiredState)
        {
            return false;
        }
    }

    return true;
}

bool UGuestParty::CanCheckIn() const
{
    return State == EGuestPartyState::CheckingIn
        && IsValid(GetLeader())
        && GetMembers().Num() == GetSize()
        && AllMembersInState(EGuestState::WaitingForCheckIn);
}

bool UGuestParty::CanAssignRoom(const ARoomActor* Room) const
{
    return CanCheckIn()
        && IsValid(Room)
        && Room->GetRoomState() == ERoomState::Available
        && Room->GetCapacity() > 0
        && IsValid(Room->GetArrivalTarget());
}

bool UGuestParty::TryAssignRoom(ARoomActor* Room)
{
    if (!CanAssignRoom(Room))
    {
        return false;
    }

    // 전원의 AI가 준비되어 있는지 먼저 확인
    for (AGuestCharacter* Member : GetMembers())
    {
        if (!IsValid(Cast<AGuestAIController>(
                Member->GetController())))
        {
            return false;
        }
    }

    const FGuid NewStayId = FGuid::NewGuid();

    // 객실 예약은 일행당 한 번
    if (!Room->TryReserve(NewStayId))
    {
        return false;
    }

    AssignedRoom = Room;
    StayId = NewStayId;
    State = EGuestPartyState::MovingToRoom;

    // 일행 전체의 잔류 여부를 한 번 결정
    bWillStayBehind =
        CheckoutPolicy == EGuestCheckoutPolicy::RequireRequest ||
        (CheckoutPolicy == EGuestCheckoutPolicy::Random &&
         (StayBehindChance >= 1.0f ||
          (StayBehindChance > 0.0f &&
           FMath::FRand() < StayBehindChance)));

    OnRoomAssigned.Broadcast(this);
    MoveMembersToRoom();

    return true;
}

void UGuestParty::MoveMembersToRoom()
{
    if (!IsValid(AssignedRoom.Get()))
    {
        return;
    }

    for (AGuestCharacter* Member : GetMembers())
    {
        if (Member->GetGuestState() == EGuestState::WaitingForCheckIn)
        {
            Member->BeginMoveTo(
                AssignedRoom->GetArrivalTarget(),
                EGuestState::MovingToRoom);
        }
    }
}

void UGuestParty::MoveMembersToLobby()
{
    for (AGuestCharacter* Member : GetMembers())
    {
        const EGuestState MemberState = Member->GetGuestState();

        if (MemberState == EGuestState::Staying ||
            MemberState == EGuestState::WaitingForCheckoutRequest)
        {
            Member->BeginMoveTo(
                LobbyTarget.Get(),
                EGuestState::MovingToLobby);
        }
    }
}

void UGuestParty::MoveMembersToExit()
{
    for (AGuestCharacter* Member : GetMembers())
    {
        const EGuestState MemberState = Member->GetGuestState();

        if (MemberState != EGuestState::MovingToExit &&
            MemberState != EGuestState::ExitCompleted &&
            MemberState != EGuestState::Inactive)
        {
            Member->BeginMoveTo(
                ExitTarget.Get(),
                EGuestState::MovingToExit);
        }
    }
}

bool UGuestParty::CanRequestCheckout() const
{
    return State == EGuestPartyState::WaitingForCheckoutRequest
        && IsValid(AssignedRoom.Get())
        && IsValid(LobbyTarget.Get());
}

bool UGuestParty::RequestCheckout()
{
    if (!CanRequestCheckout())
    {
        return false;
    }

    State = EGuestPartyState::MovingToLobby;
    MoveMembersToLobby();

    return true;
}

void UGuestParty::Advance(double CurrentSeconds)
{
    // 전원이 풀로 돌아갔거나 사라진 경우
    if (GetMembers().IsEmpty())
    {
        // 방문 도중 구성원이 모두 사라졌다면 객실 연결도 정리
        if (IsValid(AssignedRoom.Get()))
        {
            if (AssignedRoom->GetRoomState() == ERoomState::Assigned)
            {
                AssignedRoom->CancelReservation(StayId);
            }
            else if (AssignedRoom->GetRoomState() == ERoomState::Occupied)
            {
                AssignedRoom->TryEndStay(StayId);
            }
        }

        AssignedRoom = nullptr;
        State = EGuestPartyState::Finished;
        StopMovementMonitoring();
        return;
    }

    // 투숙 중 객실 자체가 사라진 경우 출구로 보냄
    if (State != EGuestPartyState::CheckingIn &&
        State != EGuestPartyState::MovingToExit &&
        State != EGuestPartyState::Finished &&
        !IsValid(AssignedRoom.Get()))
    {
        AssignedRoom = nullptr;
        State = EGuestPartyState::MovingToExit;
    }

    // 객실 배정 없이 접수 마감
    if (State == EGuestPartyState::CheckingIn &&
        CheckInDeadlineSeconds >= 0.0 &&
        CurrentSeconds >= CheckInDeadlineSeconds)
    {
        State = EGuestPartyState::MovingToExit;
    }

    if (State == EGuestPartyState::MovingToRoom)
    {
        // 이동 시작에 실패한 구성원이 있으면 재시도
        MoveMembersToRoom();

        // 전원 입실 후 객실 상태를 한 번 변경
        if (AllMembersInState(EGuestState::Staying) &&
            AssignedRoom->MarkOccupied(StayId))
        {
            State = EGuestPartyState::Staying;
        }
    }

    if (State == EGuestPartyState::Staying &&
        CheckoutAtSeconds >= 0.0 &&
        CurrentSeconds >= CheckoutAtSeconds)
    {
        if (bWillStayBehind)
        {
            State = EGuestPartyState::WaitingForCheckoutRequest;

            for (AGuestCharacter* Member : GetMembers())
            {
                Member->GuestState =
                    EGuestState::WaitingForCheckoutRequest;
            }
        }
        else
        {
            State = EGuestPartyState::MovingToLobby;
        }
    }

    if (State == EGuestPartyState::MovingToLobby)
    {
        MoveMembersToLobby();

        // 먼저 도착한 손님은 나머지 일행을 기다림
        if (AllMembersInState(EGuestState::ReadyToExit) &&
            AssignedRoom->TryEndStay(StayId))
        {
            AssignedRoom = nullptr;
            State = EGuestPartyState::MovingToExit;
        }
    }

    if (State == EGuestPartyState::MovingToExit)
    {
        MoveMembersToExit();
    }
}