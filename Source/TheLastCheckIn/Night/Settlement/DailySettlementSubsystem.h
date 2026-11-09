#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "Night/Settlement/DailySettlementTypes.h"
#include "DailySettlementSubsystem.generated.h"

class UHotelPhaseSubsystem;
class UHotelEconomySubsystem;

UCLASS()
class THELASTCHECKIN_API UDailySettlementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Result inputs only: these functions do not award money or change reputation.
	UFUNCTION(BlueprintCallable, Category="Night|Settlement")
	bool RecordAnomaly(const FDailyAnomalyResult& Result);
	UFUNCTION(BlueprintCallable, Category="Night|Settlement")
	bool RecordReputation(int32 Before, int32 After);

	// Idempotent; only succeeds after the real phase has entered Settlement.
	bool FinalizeDay();
	void SetPendingEndReason(EDayEndReason Reason);
	bool HasCurrentResult() const;
	const FDailySettlementData& GetLastResult() const { return LastResult; }

private:
	UFUNCTION() void HandleDayStarted(int32 Day);
	UFUNCTION() void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);
	bool CanRecord() const;

	UPROPERTY() TObjectPtr<UHotelPhaseSubsystem> Phase;
	UPROPERTY() TObjectPtr<UHotelEconomySubsystem> Economy;
	UPROPERTY() FDailySettlementData PendingResult;
	UPROPERTY() FDailySettlementData LastResult;
	bool bFinalized = false;
};
