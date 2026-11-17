#include "Guests/Anomaly/Behaviors/GuestDropDecorationBehavior.h"
#include "TheLastCheckIn.h"

#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/Anomaly/Props/ThrowableDecoration.h"
#include "Components/SkeletalMeshComponent.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/GuestCrowdFollowingComponent.h"
#include "Animation/AnimMontage.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Engine/World.h"

bool UGuestDropDecorationBehavior::CanStart() const
{
    if (!Super::CanStart()
        || !IsValid(ContextActor)
        || ContextActor->GetWorld() != GetWorld()
        || !IsValid(Cast<AGuestAIController>(Guest->GetController()))
        || !IsValid(Definition->InitialMontage)
        || !IsValid(Definition->ActionMontage))
    {
        return false;
    }

    const AThrowableDecoration* Prop = Cast<AThrowableDecoration>(ContextActor.Get());
    USkeletalMeshComponent* Mesh = Guest->GetMesh();

    FTransform CandidateTransform;
    return IsValid(Prop)
        && Prop->CanReserve()
        && IsValid(Mesh)
        && Mesh->DoesSocketExist(Prop->GripSocket)
        && Prop->TryBuildActionTransform(Guest.Get(), CandidateTransform);
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

    TargetProp = Cast<AThrowableDecoration>(ContextActor.Get());

    if (!IsValid(TargetProp)
        || !TargetProp->TryBuildActionTransform(Guest.Get(), ActionTransform))
    {
        RequestCancelled();
        return;
    }

    PickupTransform = FTransform(TargetProp->PickupRotation, TargetProp->PickupOffset)
        * ActionTransform;

    // 같은 물건을 여러 손님이 동시에 사용하지 않도록 예약
    if (!TargetProp->TryReserve(this))
    {
        RequestCancelled();
        return;
    }

    PropStage = EPropStage::WaitingForKnock;
    bThrowMontageStarted = false;

    UPathFollowingComponent* PathFollowing =
        GuestController->GetPathFollowingComponent();

    if (!IsValid(PathFollowing))
    {
        RequestCancelled();
        return;
    }

    // 대기용 장애물 등록을 해제한 뒤, 이동 가능한 에이전트로 다시 등록
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
            &UGuestDropDecorationBehavior::HandleMoveFinished);

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(ActionTransform.GetLocation());
    MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
    MoveRequest.SetReachTestIncludesAgentRadius(false);
    MoveRequest.SetReachTestIncludesGoalRadius(false);
    MoveRequest.SetUsePathfinding(true);
    MoveRequest.SetAllowPartialPath(false);
    // 사전 검사한 위치를 다시 투영하지 않고 그대로 목적지로 사용
    MoveRequest.SetProjectGoalLocation(false);
    MoveRequest.SetNavigationFilter(GuestController->GetDefaultNavigationFilterClass());

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
        LogGuests,
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
        !IsValid(TargetProp) ||
        !TargetProp->IsReservedBy(this) ||
        !IsValid(Definition) ||
        !IsValid(Definition->ActionMontage) ||
        !GetWorld())
    {
        RequestCancelled();
        return;
    }

    ActiveMoveId = FAIRequestID::InvalidRequest;
    GetWorld()->GetTimerManager().ClearTimer(MoveTimeoutHandle);

    const FRotator FacingRotation = ActionTransform.Rotator();

    // 이동 Success가 전달돼도 실제 목적지에 도착했는지 확인한다.
    const FVector ArrivalOffset =
        Guest->GetNavAgentLocation() - ActionTransform.GetLocation();

    // 단위: cm. 애니메이션과 실제 도착 오차를 보면서 조정한다.
    constexpr double MaxArrivalDistance = AcceptanceRadius;
    constexpr double MaxArrivalHeight = 20.0;

    const bool bTooFar =
        ArrivalOffset.SizeSquared2D() >
        FMath::Square(MaxArrivalDistance);

    const bool bWrongHeight =
        FMath::Abs(ArrivalOffset.Z) > MaxArrivalHeight;

    if (ArrivalOffset.ContainsNaN() || bTooFar || bWrongHeight)
    {
        CancelPropAction(TEXT(
            "Move reported success before reaching the decoration."));
        return;
    }

    // 가까이 도착한 경우에만 작은 위치 오차를 보정한다.
    PickupTransform.AddToTranslation(ArrivalOffset);

    Guest->SetActorRotation(FacingRotation);
    GuestController->SetControlRotation(FacingRotation);
    GuestController->UseWaitingAvoidance();

    bActing = true;

    // 첫 동작이 지정돼 있으면 한 번 재생
    if (IsValid(Definition->InitialMontage))
    {
        const float Duration = Guest->PlayAnimMontage(
            Definition->InitialMontage.Get());

        if (Duration > 0.0f)
        {
            // Throw의 Blend In 시간만큼 미리 전환
            const float BlendTime =
                Definition->ActionMontage->BlendIn.GetBlendTime();

            const float TransitionDelay =
                FMath::Max(0.01f, Duration - BlendTime);

            GetWorld()->GetTimerManager().SetTimer(
                ActionTimerHandle,
                this,
                &UGuestDropDecorationBehavior::PerformAction,
                TransitionDelay,
                false);

            return;
        }
    }

    // 첫 동작이 없거나 재생에 실패하면 바로 Throw 시작
    PerformAction();
}

void UGuestDropDecorationBehavior::PerformAction()
{
    if (!IsRunning() || !bActing)
    {
        return;
    }

    if (!CanHandlePropEvent() ||
        !IsValid(Definition) ||
        !IsValid(Definition->ActionMontage) ||
        !GetWorld())
    {
        CancelPropAction(TEXT("Invalid decoration or action montage."));
        return;
    }

    if (PropStage != EPropStage::OnGround)
    {
        CancelPropAction(TEXT("KnockDown notify did not run before transition."));
        return;
    }

    bThrowMontageStarted = true;

    const float Duration = Guest->PlayAnimMontage(
        Definition->ActionMontage.Get());

    if (Duration <= 0.0f)
    {
        CancelPropAction(TEXT("Throw montage could not play."));
    }
}

void UGuestDropDecorationBehavior::OnStopped_Implementation()
{
    bActing = false;
    bThrowMontageStarted = false;
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

        if (UGuestCrowdFollowingComponent* Crowd =
            Cast<UGuestCrowdFollowingComponent>(
                GuestController->GetPathFollowingComponent()))
        {
            Crowd->SetRequireActualArrival(false);
        }

        GuestController->UseWaitingAvoidance();
    }

    MoveFinishedHandle.Reset();

    if (IsValid(Guest) && IsValid(Definition))
    {
        if (IsValid(Definition->InitialMontage))
        {
            Guest->StopAnimMontage(
                Definition->InitialMontage.Get());
        }

        if (IsValid(Definition->ActionMontage))
        {
            Guest->StopAnimMontage(
                Definition->ActionMontage.Get());
        }
    }

    if (IsValid(TargetProp))
    {
        TargetProp->EndInteraction(this);
    }

    TargetProp = nullptr;
    ActionTransform = FTransform::Identity;
    PickupTransform = FTransform::Identity;
    PropStage = EPropStage::WaitingForKnock;
    GuestController = nullptr;

    Super::OnStopped_Implementation();
}

void UGuestDropDecorationBehavior::OnSignalReceived_Implementation(FName Signal, AActor* InstigatorActor)
{
    // 목적지 도착 전에는 제압 신호를 무시
    if (!bActing)
    {
        return;
    }

    // 도착해서 이상행동을 시작한 뒤에는 기존 제압 처리
    Super::OnSignalReceived_Implementation(Signal, InstigatorActor);
}

bool UGuestDropDecorationBehavior::CanHandlePropEvent() const
{
    return IsRunning()
        && bActing
        && IsValid(Guest)
        && IsValid(TargetProp)
        && TargetProp->IsReservedBy(this);
}

void UGuestDropDecorationBehavior::CancelPropAction(const TCHAR* Reason)
{
    UE_LOG(
        LogGuests,
        Warning,
        TEXT("DropDecoration cancelled: %s / Guest=%s"),
        Reason,
        *GetNameSafe(Guest.Get()));

    RequestCancelled();
}

void UGuestDropDecorationBehavior::HandleKnockDown()
{
    if (!CanHandlePropEvent()
        || bThrowMontageStarted
        || PropStage != EPropStage::WaitingForKnock)
    {
        return;
    }

    const FVector Velocity = ActionTransform.GetRotation().GetForwardVector()
        * TargetProp->KnockForwardSpeed + FVector::UpVector * TargetProp->KnockUpSpeed;
    if (!TargetProp->KnockDown(this, Velocity))
    {
        CancelPropAction(TEXT("KnockDown failed. Check collision and physics."));
        return;
    }

    PropStage = EPropStage::OnGround;
}

void UGuestDropDecorationBehavior::HandlePreparePickup()
{
    if (!CanHandlePropEvent()
        || !bThrowMontageStarted
        || PropStage != EPropStage::OnGround)
    {
        return;
    }

    if (!TargetProp->PreparePickup(this, PickupTransform))
    {
        CancelPropAction(TEXT("PreparePickup failed."));
        return;
    }

    PropStage = EPropStage::ReadyToGrab;
}

void UGuestDropDecorationBehavior::HandleGrab()
{
    if (!CanHandlePropEvent()
        || !bThrowMontageStarted
        || PropStage != EPropStage::ReadyToGrab)
    {
        return;
    }

    if (!TargetProp->Grab(this, Guest->GetMesh()))
    {
        CancelPropAction(TEXT("Grab failed. Check hand socket."));
        return;
    }

    PropStage = EPropStage::Holding;
}

void UGuestDropDecorationBehavior::HandleThrow()
{
    if (!CanHandlePropEvent()
        || !bThrowMontageStarted
        || PropStage != EPropStage::Holding)
    {
        return;
    }

    const FVector Velocity = ActionTransform.GetRotation().GetForwardVector()
        * TargetProp->ThrowForwardSpeed + FVector::UpVector * TargetProp->ThrowUpSpeed;
    if (!TargetProp->Throw(this, Velocity))
    {
        CancelPropAction(TEXT("Throw failed."));
        return;
    }

    PropStage = EPropStage::OnGround;
}

void UGuestDropDecorationBehavior::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    StopBehavior();
    Super::EndPlay(EndPlayReason);
}
