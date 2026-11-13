#include "Guests/Anomaly/GuestAnomalyComponent.h"

#include "Guests/Anomaly/GuestAnomalyBehavior.h"
#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/Party/GuestParty.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGuestAnomalyComponent::UGuestAnomalyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UGuestAnomalyComponent::StartAnomaly(UGuestAnomalyDefinition* InDefinition, AActor* ContextActor)
{
    // 실제 시작 직전에 다시 확인
    if (!CanStartFor(InDefinition, ContextActor))
    {
        return false;
    }

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    UGuestAnomalyBehavior* NewBehavior =
        NewObject<UGuestAnomalyBehavior>(
            Guest,
            InDefinition->BehaviorClass);

    if (!IsValid(NewBehavior))
    {
        return false;
    }

    NewBehavior->InitializeBehavior(
        this,
        Guest,
        InDefinition,
        ContextActor);

    // 개별 행동에 필요한 상세 조건
    if (!NewBehavior->CanStart())
    {
        NewBehavior->DestroyComponent();
        return false;
    }

    bTransitioning = true;

    StopGuestMovement();

    const FGuid NewParticipationId = FGuid::NewGuid();

    ParticipationId = NewParticipationId;
    bFinishIncidentOnReturn = false;
    CurrentDefinition = InDefinition;
    ActiveBehavior = NewBehavior;
    Phase = EGuestAnomalyPhase::Active;

    Guest->AddInstanceComponent(NewBehavior);
    NewBehavior->RegisterComponent();

    bTransitioning = false;

    NewBehavior->StartBehavior();

    // 시작 도중 취소되거나 다른 참여로 바뀌었는지 확인
    return ParticipationId == NewParticipationId
        && ActiveBehavior.Get() == NewBehavior
        && Phase != EGuestAnomalyPhase::Idle;
}

void UGuestAnomalyComponent::SendSignal(FName Signal, AActor* InstigatorActor)
{
    if (bTransitioning ||
        Phase != EGuestAnomalyPhase::Active ||
        !IsValid(ActiveBehavior))
    {
        return;
    }

    ActiveBehavior->ReceiveSignal(Signal, InstigatorActor);
}

void UGuestAnomalyComponent::ResolveBehavior(UGuestAnomalyBehavior* Requester)
{
    if (bTransitioning ||
        Phase != EGuestAnomalyPhase::Active ||
        Requester != ActiveBehavior.Get())
    {
        return;
    }

    bTransitioning = true;
    Phase = EGuestAnomalyPhase::ReturningToRoom;

    ActiveBehavior->StopBehavior();
    StopGuestMovement();
    ClearReturnTimer();

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    float MontageDuration = 0.0f;
    
    if (IsValid(Guest) &&
        IsValid(CurrentDefinition) &&
        IsValid(CurrentDefinition->ResolvedMontage))
    {
        MontageDuration = Guest->PlayAnimMontage(
            CurrentDefinition->ResolvedMontage.Get());
    }

    bTransitioning = false;

    
    if (!IsValid(Guest) || !GetWorld())
    {
        CancelAnomaly();
        return;
    }

    if (MontageDuration > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            ReturnTimerHandle,
            this,
            &UGuestAnomalyComponent::BeginReturnToRoom,
            MontageDuration + 0.1f,
            false);
    }
    else
    {
        // 모션이 없거나 재생에 실패하면 바로 복귀
        BeginReturnToRoom();
    }

    // OnResolved는 객실 도착 후에 알림
}

bool UGuestAnomalyComponent::CanStartFor(UGuestAnomalyDefinition* InDefinition, AActor* ContextActor) const
{
    // 이미 사건에 참여했거나 상태 변경 중
    if (bTransitioning || IsBusy())
    {
        return false;
    }

    const AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    if (!IsValid(Guest) ||
        Guest->IsActorBeingDestroyed() ||
        !IsValid(InDefinition) ||
        !InDefinition->BehaviorClass)
    {
        return false;
    }

    // 추상 행동 클래스는 실행할 수 없음
    if (InDefinition->BehaviorClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }

    if (ContextActor &&
        (!IsValid(ContextActor) ||
         ContextActor->GetWorld() != GetWorld()))
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    return IsValid(Party)
        && Party->CanParticipateInAnomaly()
        && Guest->GetGuestState() == EGuestState::Staying;
}

bool UGuestAnomalyComponent::FinishIncident()
{
    if (bTransitioning ||
        Phase != EGuestAnomalyPhase::Resolved)
    {
        return false;
    }

    bTransitioning = true;

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (IsValid(Guest) &&
        IsValid(CurrentDefinition) &&
        IsValid(CurrentDefinition->ResolvedMontage))
    {
        Guest->StopAnimMontage(
            CurrentDefinition->ResolvedMontage.Get());
    }

    DestroyBehavior();

    CurrentDefinition = nullptr;
    Phase = EGuestAnomalyPhase::Idle;
    bFinishIncidentOnReturn = false;

    bTransitioning = false;

    return true;
}

void UGuestAnomalyComponent::ReturnToRoomAfterIncidentEnd()
{
    if (bTransitioning || !IsBusy())
    {
        return;
    }

    if (Phase == EGuestAnomalyPhase::Resolved)
    {
        FinishIncident();
        return;
    }

    bFinishIncidentOnReturn = true;

    // 기존 복귀 이동이나 해결 모션 이후의 복귀 예약을 취소하지 않는다.
    if (Phase == EGuestAnomalyPhase::ReturningToRoom)
    {
        return;
    }

    bTransitioning = true;
    Phase = EGuestAnomalyPhase::ReturningToRoom;

    DestroyBehavior();
    StopGuestMovement();
    ClearReturnTimer();

    bTransitioning = false;

    UE_LOG(LogTemp, Log,
        TEXT("Anomaly guest returning after settlement: %s"),
        *GetNameSafe(GetOwner()));

    BeginReturnToRoom();
}

void UGuestAnomalyComponent::CancelAnomaly()
{
    if (bTransitioning || !IsBusy())
    {
        return;
    }

    bTransitioning = true;

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (IsValid(Guest) &&
        IsValid(CurrentDefinition) &&
        IsValid(CurrentDefinition->ResolvedMontage))
    {
        Guest->StopAnimMontage(
            CurrentDefinition->ResolvedMontage.Get());
    }

    ClearReturnTimer();

    DestroyBehavior();
    StopGuestMovement();

    // Phase를 Idle로 바꾸기 전에 복귀 상태를 정리
    if (IsValid(Guest))
    {
        Guest->ClearAnomalyReturnState();
    }

    CurrentDefinition = nullptr;
    Phase = EGuestAnomalyPhase::Idle;
    bFinishIncidentOnReturn = false;

    bTransitioning = false;

    OnCancelled.Broadcast(this);
}

void UGuestAnomalyComponent::StopGuestMovement()
{
    const AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    if (!IsValid(Guest))
    {
        return;
    }

    AGuestAIController* Controller =
        Cast<AGuestAIController>(Guest->GetController());

    if (IsValid(Controller))
    {
        Controller->StopGuestAI();
        Controller->UseWaitingAvoidance();
    }
}

void UGuestAnomalyComponent::DestroyBehavior()
{
    UGuestAnomalyBehavior* OldBehavior = ActiveBehavior.Get();
    ActiveBehavior = nullptr;

    if (!IsValid(OldBehavior))
    {
        return;
    }

    OldBehavior->StopBehavior();

    if (AActor* Owner = OldBehavior->GetOwner())
    {
        Owner->RemoveInstanceComponent(OldBehavior);
    }

    OldBehavior->DestroyComponent();
}

void UGuestAnomalyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearReturnTimer();
    CancelAnomaly();

    Super::EndPlay(EndPlayReason);
}

void UGuestAnomalyComponent::BeginReturnToRoom()
{
    ClearReturnTimer();

    if (bTransitioning ||
        Phase != EGuestAnomalyPhase::ReturningToRoom)
    {
        return;
    }

    AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    UWorld* World = GetWorld();

    if (!IsValid(Guest) || !World)
    {
        CancelAnomaly();
        return;
    }

    // 도중에 막혀도 사건이 무한정 유지되지 않게 제한
    World->GetTimerManager().SetTimer(
        ReturnTimerHandle,
        this,
        &UGuestAnomalyComponent::HandleReturnTimeout,
        FMath::Max(1.0f, ReturnTimeoutSeconds),
        false);

    if (!Guest->ReturnToAssignedRoomAfterAnomaly())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Anomaly room return could not start: %s"),
            *GetNameSafe(Guest));

        CancelAnomaly();
    }
}

void UGuestAnomalyComponent::NotifyReturnedToRoom()
{
    if (bTransitioning ||
        Phase != EGuestAnomalyPhase::ReturningToRoom)
    {
        return;
    }

    const AGuestCharacter* Guest =
        Cast<AGuestCharacter>(GetOwner());

    if (!IsValid(Guest) ||
        Guest->GetGuestState() != EGuestState::Staying)
    {
        return;
    }

    ClearReturnTimer();
    Phase = EGuestAnomalyPhase::Resolved;

    UE_LOG(LogTemp, Log,
        TEXT("Anomaly guest returned to room: %s"),
        *GetNameSafe(Guest));

    if (bFinishIncidentOnReturn)
    {
        FinishIncident();
        return;
    }

    OnResolved.Broadcast(this);
}

void UGuestAnomalyComponent::HandleReturnTimeout()
{
    if (Phase != EGuestAnomalyPhase::ReturningToRoom)
    {
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("Anomaly room return timed out: %s"),
        *GetNameSafe(GetOwner()));

    CancelAnomaly();
}

void UGuestAnomalyComponent::ClearReturnTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReturnTimerHandle);
    }
}
