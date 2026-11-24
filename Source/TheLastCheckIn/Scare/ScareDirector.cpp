#include "Scare/ScareDirector.h"
#include "Scare/ScareBase.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "TimerManager.h"

AScareDirector::AScareDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AScareDirector::BeginPlay()
{
	Super::BeginPlay();
	if (UHotelPhaseSubsystem* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
	{
		Phase->OnPhaseChanged.AddDynamic(this, &AScareDirector::HandlePhaseChanged);
		Phase->OnDaySlotChanged.AddDynamic(this, &AScareDirector::HandleSlotChanged);
	}
}

void AScareDirector::HandlePhaseChanged(EHotelPhase, EHotelPhase) { TryFire(); }
void AScareDirector::HandleSlotChanged(EDaySlot, EDaySlot) { TryFire(); }

void AScareDirector::TryFire()
{
	const UHotelPhaseSubsystem* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>();
	if (!Phase) return;

	for (FScareEntry& Entry : Schedule)
	{
		if (Entry.bFired || !Entry.Scare) continue;
		if (Entry.Day != Phase->GetCurrentDay()) continue;
		if (Entry.Phase != Phase->GetCurrentPhase()) continue;
		if (Entry.Phase == EHotelPhase::Day && Entry.Slot != Phase->GetCurrentDaySlot()) continue;

		Entry.bFired = true;
		TWeakObjectPtr<AScareBase> Target = Entry.Scare;
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, [Target]()
		{
			if (Target.IsValid()) Target->PlayScare();
		}, FMath::FRandRange(Entry.MinDelay, Entry.MaxDelay), false);
	}
}