#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "Night/Anomaly/AnomalyTypes.h"
#include "AnomalyManagerSubsystem.generated.h"

class AAnomalyBase;
class UGameClockSubsystem;
class UHotelPhaseSubsystem;
class UDailySettlementSubsystem;

UCLASS()
class THELASTCHECKIN_API UAnomalyManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	void RegisterAnomaly(AAnomalyBase* Anomaly);

	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	bool StartAnomalyById(FName AnomalyId);
	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	AAnomalyBase* StartRandomAnomaly();

	UFUNCTION(BlueprintPure, Category="Night|Anomaly")
	AAnomalyBase* FindAnomaly(FName AnomalyId) const;
	UFUNCTION(BlueprintPure, Category="Night|Anomaly")
	TArray<AAnomalyBase*> GetRunningAnomalies() const;

private:
	UFUNCTION() void HandleAnomalyStateChanged(AAnomalyBase* Anomaly, EAnomalyState OldState, EAnomalyState NewState);
	UFUNCTION() void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);
	UFUNCTION() void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);
	void RecordResult(const AAnomalyBase* Anomaly);
	void FailRunningAnomalies();
	
	UFUNCTION() void HandleDayStarted(int32 Day);
	double GetNightSeconds(int32 Day, int32 Hour) const;

	TMap<TWeakObjectPtr<AAnomalyBase>, double> PlannedStartSeconds;

	UPROPERTY() TObjectPtr<UGameClockSubsystem> Clock;
	UPROPERTY() TObjectPtr<UHotelPhaseSubsystem> Phase;
	UPROPERTY() TObjectPtr<UDailySettlementSubsystem> Settlement;
	UPROPERTY() TArray<TWeakObjectPtr<AAnomalyBase>> Anomalies;
	TMap<TWeakObjectPtr<AAnomalyBase>, double> StartSeconds;
};
