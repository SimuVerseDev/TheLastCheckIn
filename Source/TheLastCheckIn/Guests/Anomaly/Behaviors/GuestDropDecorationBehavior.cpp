#include "Guests/Anomaly/Behaviors/GuestDropDecorationBehavior.h"

#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Animation/AnimMontage.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Engine/World.h"

bool UGuestDropDecorationBehavior::CanStart() const
{
    return Super::CanStart()
        && IsValid(ContextActor)
        && ContextActor->GetWorld() == GetWorld()
        && IsValid(Cast<AGuestAIController>(Guest->GetController()));
}

void UGuestDropDecorationBehavior::OnStarted_Implementation()
{
    Super::OnStarted_Implementation();

    GuestController =
        Cast<AGuestAIController>(Guest->GetController());

    if (!IsValid(GuestController) ||
        !IsValid(ContextActor) ||
        !GetWorld())
    {
        RequestCancelled();
        return;
    }

    UPathFollowingComponent* PathFollowing =
        GuestController->GetPathFollowingComponent();

    if (!IsValid(PathFollowing))
    {
        RequestCancelled();
        return;
    }

    // 대기용 장애물 등록을 해제한 뒤, 이동 가능한 에이전트로 다시 등록
    if (UCrowdFollowingComponent* Crowd =
        Cast<UCrowdFollowingComponent>(PathFollowing))
    {
        Crowd->SetCrowdSimulationState(
            ECrowdSimulationState::Disabled);

        Crowd->SuspendCrowdSteering(false);

        Crowd->SetCrowdSimulationState(
            ECrowdSimulationState::Enabled);
    }

    MoveFinishedHandle =
        PathFollowing->OnRequestFinished.AddUObject(
            this,
            &UGuestDropDecorationBehavior::HandleMoveFinished);

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(ContextActor->GetActorLocation());
    MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetAllowPartialPath(false);
    MoveRequest.SetProjectGoalLocation(true);

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
        &UGuestDropDecorationBehavior::HandleMoveTimeout,
        MoveTimeoutSeconds,
        false);
}

void UGuestDropDecorationBehavior::HandleMoveFinished(
    FAIRequestID RequestId,
    const FPathFollowingResult& Result)
{
    // 현재 행동이 요청한 이동만 처리
    if (!IsRunning() ||
        ActiveMoveId == FAIRequestID::InvalidRequest ||
        RequestId != ActiveMoveId)
    {
        return;
    }

    ActiveMoveId = FAIRequestID::InvalidRequest;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MoveTimeoutHandle);
    }

    if (Result.Code != EPathFollowingResult::Success)
    {
        RequestCancelled();
        return;
    }

    BeginActing();
}

void UGuestDropDecorationBehavior::HandleMoveTimeout()
{
    if (!IsRunning())
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("DropDecoration movement timed out: %s"),
        *GetNameSafe(Guest.Get()));

    RequestCancelled();
}

void UGuestDropDecorationBehavior::BeginActing()
{
    if (!IsRunning() || bActing)
    {
        return;
    }

    if (!IsValid(Guest) ||
        !IsValid(GuestController) ||
        !IsValid(ContextActor) ||
        !GetWorld())
    {
        RequestCancelled();
        return;
    }

    ActiveMoveId = FAIRequestID::InvalidRequest;
    GetWorld()->GetTimerManager().ClearTimer(MoveTimeoutHandle);

    const FRotator FacingRotation(
        0.0f,
        ContextActor->GetActorRotation().Yaw,
        0.0f);

    Guest->SetActorRotation(FacingRotation);
    GuestController->SetControlRotation(FacingRotation);
    GuestController->UseWaitingAvoidance();

    bActing = true;

    PerformAction();
}

void UGuestDropDecorationBehavior::PerformAction()
{
    if (!IsRunning() || !bActing)
    {
        return;
    }

    if (!IsValid(Guest) ||
        !IsValid(Definition) ||
        !IsValid(ContextActor) ||
        !GetWorld())
    {
        RequestCancelled();
        return;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("DropDecoration action: %s"),
        *GetNameSafe(Guest.Get()));

    float MontageDuration = 0.0f;

    if (IsValid(Definition->ActionMontage))
    {
        MontageDuration = Guest->PlayAnimMontage(
            Definition->ActionMontage.Get());
    }

    // 재생 중인 몽타주가 반복 호출로 끊기지 않도록 함
    const float NextDelay = FMath::Max(
        FMath::Max(0.1f, Definition->ActionIntervalSeconds),
        MontageDuration + 0.1f);

    GetWorld()->GetTimerManager().SetTimer(
        ActionTimerHandle,
        this,
        &UGuestDropDecorationBehavior::PerformAction,
        NextDelay,
        false);
}

void UGuestDropDecorationBehavior::OnStopped_Implementation()
{
    bActing = false;
    ActiveMoveId = FAIRequestID::InvalidRequest;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MoveTimeoutHandle);
        World->GetTimerManager().ClearTimer(ActionTimerHandle);
    }

    if (IsValid(GuestController))
    {
        if (UPathFollowingComponent* PathFollowing =
            GuestController->GetPathFollowingComponent())
        {
            PathFollowing->OnRequestFinished.Remove(MoveFinishedHandle);
        }

        GuestController->StopMovement();
        GuestController->UseWaitingAvoidance();
    }

    MoveFinishedHandle.Reset();

    if (IsValid(Guest) &&
        IsValid(Definition) &&
        IsValid(Definition->ActionMontage))
    {
        Guest->StopAnimMontage(
            Definition->ActionMontage.Get());
    }

    GuestController = nullptr;

    Super::OnStopped_Implementation();
}