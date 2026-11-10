#include "Guests/Anomaly/GuestAnomalyBehavior.h"

#include "Guests/Anomaly/GuestAnomalyComponent.h"
#include "Guests/Anomaly/GuestAnomalyDefinition.h"
#include "Guests/GuestCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGuestAnomalyBehavior::UGuestAnomalyBehavior()
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = false;
}

void UGuestAnomalyBehavior::InitializeBehavior(
    UGuestAnomalyComponent* InHost,
    AGuestCharacter* InGuest,
    UGuestAnomalyDefinition* InDefinition,
    AActor* InContext)
{
    Host = InHost;
    Guest = InGuest;
    Definition = InDefinition;
    ContextActor = InContext;
}

bool UGuestAnomalyBehavior::CanStart() const
{
    return IsValid(Host)
        && IsValid(Guest)
        && IsValid(Definition);
}

void UGuestAnomalyBehavior::StartBehavior()
{
    if (bRunning || !CanStart())
    {
        return;
    }

    ReceivedSignals = 0;
    bRunning = true;

    OnStarted();
}

void UGuestAnomalyBehavior::StopBehavior()
{
    if (!bRunning)
    {
        return;
    }
    
    bRunning = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }

    OnStopped();
}

void UGuestAnomalyBehavior::ReceiveSignal(
    FName Signal,
    AActor* InstigatorActor)
{
    if (!bRunning)
    {
        return;
    }

    OnSignalReceived(Signal, InstigatorActor);
}

void UGuestAnomalyBehavior::OnStarted_Implementation()
{
    // 개별 행동의 C++ 자식에서 구현
}

void UGuestAnomalyBehavior::OnStopped_Implementation()
{
    // 개별 행동의 C++ 자식에서 구현
}

void UGuestAnomalyBehavior::OnSignalReceived_Implementation(
    FName Signal,
    AActor* InstigatorActor)
{
    if (!IsValid(Definition) ||
        Signal != Definition->ResolutionSignal)
    {
        return;
    }

    ++ReceivedSignals;

    if (ReceivedSignals >= FMath::Max(1, Definition->RequiredSignals))
    {
        RequestResolved();
    }
}

void UGuestAnomalyBehavior::RequestResolved()
{
    if (bRunning && IsValid(Host))
    {
        Host->ResolveBehavior(this);
    }
}

void UGuestAnomalyBehavior::RequestCancelled()
{
    if (bRunning && IsValid(Host))
    {
        Host->CancelAnomaly();
    }
}