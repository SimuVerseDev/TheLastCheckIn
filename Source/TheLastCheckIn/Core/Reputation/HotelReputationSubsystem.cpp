#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Core/Reputation/ReputationSettings.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Night/Settlement/DailySettlementSubsystem.h"

void UHotelReputationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	Settlement = Collection.InitializeDependency<UDailySettlementSubsystem>();
	Reputation = UReputationSettings::Get()->StartReputation;
	Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
	HandleDayStarted(Phase->GetCurrentDay());
}

void UHotelReputationSubsystem::Deinitialize()
{
	if (Phase)
	{
		Phase->OnDayStarted.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UHotelReputationSubsystem::AddReputation(EReputationReason Reason)
{
	if (const int32* Delta = UReputationSettings::Get()->ReasonDeltas.Find(Reason))
	{
		ApplyDelta(*Delta, Reason); 
	}
}

void UHotelReputationSubsystem::ApplyRoomCleanliness(float Cleanliness)
{
	const UReputationSettings* Settings = UReputationSettings::Get();
	if (Cleanliness >= Settings->CleanBonusThreshold)
	{
		AddReputation(EReputationReason::CleanRoomAssigned);
	}
	else if (Cleanliness < Settings->DirtyPenaltyThreshold)
	{
		AddReputation(EReputationReason::DirtyRoomAssigned);
	}
}

bool UHotelReputationSubsystem::TryApplyForcedCheckout()
{
	if (FMath::FRand() < UReputationSettings::Get()->ForcedCheckoutPenaltyChance)
	{
		AddReputation(EReputationReason::ForcedCheckout);
		return true;
	}
	return false;
}

int32 UHotelReputationSubsystem::GetStarLevel() const
{
	const UReputationSettings* Settings = UReputationSettings::Get();
	const int32 MaxStar = Settings->MaxReputation / Settings->ReputationPerStar;
	return FMath::Clamp(Reputation / Settings->ReputationPerStar, 0, MaxStar);
}

float UHotelReputationSubsystem::GetRoomRateMultiplier() const
{
	const TArray<float>& Multipliers = UReputationSettings::Get()->StarRateMultipliers;
	if (Multipliers.IsEmpty())
	{
		return 1.0f;
	}
	const int32 Star = GetStarLevel();
	return Multipliers.IsValidIndex(Star) ? Multipliers[Star] : Multipliers.Last();
}

void UHotelReputationSubsystem::HandleDayStarted(int32 Day)
{
	DayStartReputation = Reputation;
	if (Settlement)
	{
		Settlement->RecordReputation(DayStartReputation, Reputation);
	}
}

void UHotelReputationSubsystem::ApplyDelta(int32 Delta, EReputationReason Reason)
{
	if (bFired || Delta == 0)
	{
		return;
	}
	const int32 OldValue = Reputation;
	const int32 OldStar = GetStarLevel();
	Reputation = FMath::Min(Reputation + Delta, UReputationSettings::Get()->MaxReputation);
	
	if (Reputation == OldValue)
	{
		return;
	}
	OnReputationChanged.Broadcast(OldValue, Reputation, Reason);
	
	const int32 NewStar = GetStarLevel();
	if (NewStar != OldStar)
	{
		OnStarLevelChanged.Broadcast(OldStar, NewStar);
	}

	if (Settlement)
	{
		Settlement->RecordReputation(DayStartReputation, Reputation);
	}

	if (Reputation < 0)
	{
		bFired = true;
		OnReputationDepleted.Broadcast();
	}
}

