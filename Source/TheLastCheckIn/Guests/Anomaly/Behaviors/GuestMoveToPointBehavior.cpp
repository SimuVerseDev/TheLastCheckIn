#include "Guests/Anomaly/Behaviors/GuestMoveToPointBehavior.h"

#include "TheLastCheckIn.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/GuestCrowdFollowingComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Engine/World.h"

bool UGuestMoveToPointBehavior::CanStart() const
{
    return Super::CanStart()
        && IsValid(ContextActor)
        && ContextActor->GetWorld() == GetWorld()
        && IsValid(Cast<AGuestAIController>(Guest->GetController()));
}

bool UGuestMoveToPointBehavior::InitializeMovement()
{
    GuestController = IsValid(Guest)
        ? Cast<AGuestAIController>(Guest->GetController())
        : nullptr;

    return HasMovementContext();
}

bool UGuestMoveToPointBehavior::HasMovementContext() const
{
    return IsValid(Guest)
        && IsValid(GuestController)
        && IsValid(ContextActor)
        && GetWorld() != nullptr;
}

void UGuestMoveToPointBehavior::MoveToActionPoint()
{
    UPathFollowingComponent* PathFollowing =
        GuestController->GetPathFollowingComponent();

    if (!IsValid(PathFollowing))
    {
        RequestCancelled();
        return;
    }

    // 기존 순서대로 대기 장애물 상태에서 이동 상태로 전환
    if (UGuestCrowdFollowingComponent* Crowd =
        Cast<UGuestCrowdFollowingComponent>(PathFollowing))
    {
        Crowd->SetCrowdSimulationState(
            ECrowdSimulationState::Disabled);

        Crowd->SuspendCrowdSteering(false);

        Crowd->SetCrowdSimulationState(
            ECrowdSimulationState::Enabled);

        Crowd->SetRequireActualArrival(true);
    }

    MoveFinishedHandle =
        PathFollowing->OnRequestFinished.AddUObject(
            this,
            &UGuestMoveToPointBehavior::HandleMoveFinished);

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(ActionTransform.GetLocation());
    MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
    MoveRequest.SetReachTestIncludesAgentRadius(false);
    MoveRequest.SetReachTestIncludesGoalRadius(false);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetAllowPartialPath(false);

    // 지정한 목적지를 다른 위치로 다시 투영하지 않음
    MoveRequest.SetProjectGoalLocation(false);

    MoveRequest.SetNavigationFilter(
        GuestController->GetDefaultNavigationFilterClass());

    const FPathFollowingRequestResult MoveResult =
        GuestController->MoveTo(MoveRequest);

    if (MoveResult.Code == EPathFollowingRequestResult::Failed)
    {
        RequestCancelled();
        return;
    }

    if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        BeginActing();
        return;
    }

    ActiveMoveId = MoveResult.MoveId;

    GetWorld()->GetTimerManager().SetTimer(
        MoveTimeoutHandle,
        this,
        &UGuestMoveToPointBehavior::HandleMoveTimeout,
        MoveTimeoutSeconds,
        false);
}

void UGuestMoveToPointBehavior::HandleMoveFinished(
    FAIRequestID RequestId,
    const FPathFollowingResult& Result)
{
    // 현재 행동이 요청한 이동만 처리
    if (!IsRunning()
        || ActiveMoveId == FAIRequestID::InvalidRequest
        || RequestId != ActiveMoveId)
    {
        return;
    }

    ClearMoveRequest();

    if (Result.Code != EPathFollowingResult::Success)
    {
        RequestCancelled();
        return;
    }

    BeginActing();
}

void UGuestMoveToPointBehavior::HandleMoveTimeout()
{
    if (!IsRunning())
    {
        return;
    }

    UE_LOG(
        LogGuests,
        Warning,
        TEXT("Guest action movement timed out: %s / Behavior=%s"),
        *GetNameSafe(Guest.Get()),
        *GetNameSafe(GetClass()));

    RequestCancelled();
}

bool UGuestMoveToPointBehavior::TryConfirmArrival(FVector& OutArrivalOffset)
{
    ClearMoveRequest();

    OutArrivalOffset =
        Guest->GetNavAgentLocation() - ActionTransform.GetLocation();

    // 도착 허용 오차
    constexpr double MaxArrivalDistance = AcceptanceRadius;
    constexpr double MaxArrivalHeight = 20.0;

    const bool bTooFar =
        OutArrivalOffset.SizeSquared2D()
        > FMath::Square(MaxArrivalDistance);

    const bool bWrongHeight =
        FMath::Abs(OutArrivalOffset.Z) > MaxArrivalHeight;

    return !OutArrivalOffset.ContainsNaN()
        && !bTooFar
        && !bWrongHeight;
}

void UGuestMoveToPointBehavior::ApplyActionFacing()
{
    const FRotator FacingRotation = ActionTransform.Rotator();

    Guest->SetActorRotation(FacingRotation);
    GuestController->SetControlRotation(FacingRotation);
    GuestController->UseWaitingAvoidance();
}

void UGuestMoveToPointBehavior::ClearMoveRequest()
{
    ActiveMoveId = FAIRequestID::InvalidRequest;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MoveTimeoutHandle);
    }
}

void UGuestMoveToPointBehavior::StopPointMovement()
{
    if (IsValid(GuestController))
    {
        // 이동 정지 전에 완료 콜백부터 해제
        if (UPathFollowingComponent* PathFollowing =
            GuestController->GetPathFollowingComponent())
        {
            PathFollowing->OnRequestFinished.Remove(MoveFinishedHandle);
        }

        GuestController->StopMovement();

        if (UGuestCrowdFollowingComponent* Crowd =
            Cast<UGuestCrowdFollowingComponent>(
                GuestController->GetPathFollowingComponent()))
        {
            Crowd->SetRequireActualArrival(false);
        }

        GuestController->UseWaitingAvoidance();
    }

    MoveFinishedHandle.Reset();
}

void UGuestMoveToPointBehavior::OnSignalReceived_Implementation(
    FName Signal,
    AActor* InstigatorActor)
{
    // 이동 중 타격은 해결 횟수에 누적하지 않음
    if (!bActing)
    {
        return;
    }

    Super::OnSignalReceived_Implementation(Signal, InstigatorActor);
}

void UGuestMoveToPointBehavior::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopBehavior();
    Super::EndPlay(EndPlayReason);
}
