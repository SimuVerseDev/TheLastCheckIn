#include "Guests/Anomaly/Behaviors/GuestDropDecorationBehavior.h"

#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/Anomaly/Props/ThrowableDecoration.h"
#include "Components/SkeletalMeshComponent.h"
#include "Guests/GuestCharacter.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"

bool UGuestDropDecorationBehavior::CanStart() const
{
    // 컨텍스트와 AI 컨트롤러 검사는 공통 부모에서 처리
    if (!Super::CanStart()
        || !IsValid(Definition->InitialMontage)
        || !IsValid(Definition->ActionMontage))
    {
        return false;
    }

    const AThrowableDecoration* Prop =
        Cast<AThrowableDecoration>(ContextActor.Get());

    USkeletalMeshComponent* Mesh = Guest->GetMesh();

    FTransform CandidateTransform;

    return IsValid(Prop)
        && Prop->CanReserve()
        && IsValid(Mesh)
        && Mesh->DoesSocketExist(Prop->GripSocket)
        && Prop->TryBuildActionTransform(
            Guest.Get(),
            CandidateTransform);
}

void UGuestDropDecorationBehavior::OnStarted_Implementation()
{
    Super::OnStarted_Implementation();

    // 기존과 같은 위치에서 컨트롤러와 이동 대상을 확인
    if (!InitializeMovement())
    {
        RequestCancelled();
        return;
    }

    TargetProp = Cast<AThrowableDecoration>(ContextActor.Get());

    if (!IsValid(TargetProp)
        || !TargetProp->TryBuildActionTransform(
            Guest.Get(),
            ActionTransform))
    {
        RequestCancelled();
        return;
    }

    PickupTransform =
        FTransform(
            TargetProp->PickupRotation,
            TargetProp->PickupOffset)
        * ActionTransform;

    // 위치 계산 후 장식물을 예약
    if (!TargetProp->TryReserve(this))
    {
        RequestCancelled();
        return;
    }

    PropStage = EPropStage::WaitingForKnock;
    bThrowMontageStarted = false;

    // 이 아래에 있던 이동 코드를 공통 부모로 추출
    MoveToActionPoint();
}

void UGuestDropDecorationBehavior::BeginActing()
{
    if (!IsRunning() || bActing)
    {
        return;
    }

    if (!HasMovementContext()
        || !IsValid(TargetProp)
        || !TargetProp->IsReservedBy(this)
        || !IsValid(Definition)
        || !IsValid(Definition->ActionMontage))
    {
        RequestCancelled();
        return;
    }

    FVector ArrivalOffset;

    if (!TryConfirmArrival(ArrivalOffset))
    {
        CancelPropAction(TEXT(
            "Move reported success before reaching the decoration."));
        return;
    }

    // 기존처럼 방향 적용 전에 집기 위치를 보정
    PickupTransform.AddToTranslation(ArrivalOffset);

    ApplyActionFacing();

    // 위치 보정과 방향 적용이 끝난 뒤에만 제압 허용
    bActing = true;

    if (IsValid(Definition->InitialMontage))
    {
        const float Duration =
            Guest->PlayAnimMontage(Definition->InitialMontage.Get());

        if (Duration > 0.0f)
        {
            // 기존 전환 시점을 유지
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

    // 이동 요청과 이동 타이머부터 정리
    ClearMoveRequest();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ActionTimerHandle);
    }

    // 애니메이션과 장식물보다 이동을 먼저 정리
    StopPointMovement();

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
        LogTemp,
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

