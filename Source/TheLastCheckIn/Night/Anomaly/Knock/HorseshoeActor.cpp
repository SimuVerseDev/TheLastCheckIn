#include "Night/Anomaly/Knock/HorseshoeActor.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"

void AHorseshoeActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (UHotelPhaseSubsystem* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
	{
		Phase->OnDayStarted.AddDynamic(this, &ThisClass::AHorseshoeActor::HandleDayStarted);
	}
}

void AHorseshoeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHotelPhaseSubsystem* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
	{
		Phase->OnDayStarted.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AHorseshoeActor::HandleDayStarted(int32 Day)
{
	if (bUsed)
	{
		Destroy();
	}
}