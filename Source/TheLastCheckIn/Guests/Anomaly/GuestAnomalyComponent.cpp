#include "Guests/Anomaly/GuestAnomalyComponent.h"

#include "Guests/Anomaly/GuestAnomalyBehavior.h"
#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/Party/GuestParty.h"
#include "Animation/AnimMontage.h"

UGuestAnomalyComponent::UGuestAnomalyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UGuestAnomalyComponent::StartAnomaly(
    UGuestAnomalyDefinition* InDefinition,
    AActor* ContextActor)
{
    if (bTransitioning || IsBusy())
    {
        return false;
    }

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (!IsValid(Guest) ||
        !IsValid(InDefinition) ||
        !InDefinition->BehaviorClass)
    {
        return false;
    }

    if (InDefinition->BehaviorClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    if (!IsValid(Party) ||
        !Party->CanParticipateInAnomaly() ||
        Guest->GetGuestState() != EGuestState::Staying)
    {
        return false;
    }

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

    // 행동 자체에 필요한 설정을 먼저 검사
    if (!NewBehavior->CanStart())
    {
        NewBehavior->DestroyComponent();
        return false;
    }

    bTransitioning = true;

    StopGuestMovement();

    CurrentDefinition = InDefinition;
    ActiveBehavior = NewBehavior;
    Phase = EGuestAnomalyPhase::Active;

    Guest->AddInstanceComponent(NewBehavior);
    NewBehavior->RegisterComponent();

    bTransitioning = false;

    NewBehavior->StartBehavior();
    
    return ActiveBehavior.Get() == NewBehavior &&
        Phase != EGuestAnomalyPhase::Idle;
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
    Phase = EGuestAnomalyPhase::Resolved;

    ActiveBehavior->StopBehavior();
    StopGuestMovement();

    AGuestCharacter* Guest = Cast<AGuestCharacter>(GetOwner());

    if (IsValid(Guest) &&
        IsValid(CurrentDefinition) &&
        IsValid(CurrentDefinition->ResolvedMontage))
    {
        Guest->PlayAnimMontage(
            CurrentDefinition->ResolvedMontage.Get());
    }

    bTransitioning = false;

    OnResolved.Broadcast(this);
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

    bTransitioning = false;

    return true;
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

    DestroyBehavior();
    StopGuestMovement();

    CurrentDefinition = nullptr;
    Phase = EGuestAnomalyPhase::Idle;

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

void UGuestAnomalyComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    CancelAnomaly();

    Super::EndPlay(EndPlayReason);
}