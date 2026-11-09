#include "Night/Settlement/DailySettlementSubsystem.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"

void UDailySettlementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	Economy = Collection.InitializeDependency<UHotelEconomySubsystem>();
	Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
	Phase->OnPhaseChanged.AddDynamic(this, &ThisClass::HandlePhaseChanged);
	HandleDayStarted(Phase->GetCurrentDay());
}

void UDailySettlementSubsystem::Deinitialize()
{
	if (Phase)
	{
		Phase->OnDayStarted.RemoveAll(this);
		Phase->OnPhaseChanged.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UDailySettlementSubsystem::HandleDayStarted(int32 Day)
{
	PendingResult = FDailySettlementData();
	PendingResult.Day = Day;
	bFinalized = false;
	// LastResult deliberately survives the next day's income reset.
}

bool UDailySettlementSubsystem::CanRecord() const
{
	return Phase && !bFinalized && Phase->GetCurrentPhase() != EHotelPhase::Settlement;
}

bool UDailySettlementSubsystem::RecordAnomaly(const FDailyAnomalyResult& Result)
{
	if (!CanRecord() || Result.Id.IsNone()) return false;
	FDailyAnomalyResult* Existing = PendingResult.Anomalies.FindByPredicate(
		[&Result](const FDailyAnomalyResult& Item) { return Item.Id == Result.Id; });
	if (Existing) *Existing = Result;
	else PendingResult.Anomalies.Add(Result);
	return true;
}

bool UDailySettlementSubsystem::RecordReputation(int32 Before, int32 After)
{
	if (!CanRecord()) return false;
	PendingResult.bHasReputation = true;
	PendingResult.ReputationBefore = Before;
	PendingResult.ReputationAfter = After;
	return true;
}

void UDailySettlementSubsystem::SetPendingEndReason(EDayEndReason Reason)
{
	if (CanRecord()) PendingResult.EndReason = Reason;
}

bool UDailySettlementSubsystem::HasCurrentResult() const
{
	return bFinalized && Phase && LastResult.Day == Phase->GetCurrentDay();
}

bool UDailySettlementSubsystem::FinalizeDay()
{
	if (!Phase || !Economy || Phase->GetCurrentPhase() != EHotelPhase::Settlement) return false;
	if (HasCurrentResult()) return true;
	PendingResult.Day = Phase->GetCurrentDay();
	PendingResult.Income = Economy->GetTodayIncome();
	PendingResult.Capital = Economy->GetCapital();
	LastResult = PendingResult;
	bFinalized = true;
	return true;
}

void UDailySettlementSubsystem::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase == EHotelPhase::Settlement) FinalizeDay();
}
