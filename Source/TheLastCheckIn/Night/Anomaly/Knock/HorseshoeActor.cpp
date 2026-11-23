#include "Night/Anomaly/Knock/HorseshoeActor.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Night/Anomaly/Knock/KnockDoorActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

AHorseshoeActor::AHorseshoeActor()
{
	bRestUntilPickedUp = true;
	Mesh->BodyInstance.bUseCCD = true;
}

void AHorseshoeActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (UHotelPhaseSubsystem* Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>())
	{
		Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
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

void AHorseshoeActor::NotifyReleased(bool bThrown)
{
	Super::NotifyReleased(bThrown);

	if (bThrown || bUsed)
	{
		return;
	}
	for (TActorIterator<AKnockDoorActor> It(GetWorld()); It; ++It)
	{
		if (It->TryPlaceHorseshoe(this))
		{
			break;
		}
	}
}