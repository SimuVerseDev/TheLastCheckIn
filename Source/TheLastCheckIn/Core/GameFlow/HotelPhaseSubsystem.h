#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "HotelPhaseSubsystem.generated.h"

class UGameClockSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHotelPhaseChanged,
	EHotelPhase, OldPhase,
	EHotelPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FDaySlotChanged,
	EDaySlot, OldSlot,
	EDaySlot, NewSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FHotelDayEvent,
	int32, Day);

UCLASS()
class THELASTCHECKIN_API UHotelPhaseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable, Category = "Game|Phase")
	void StartRun();
	
	UFUNCTION(BlueprintCallable, Category = "Game|Phase")
	void ConfirmSettlement();
	
	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	EHotelPhase GetCurrentPhase() const
	{
		return CurrentPhase;
	}
	
	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	EDaySlot GetCurrentDaySlot() const
	{
		return CurrentDaySlot;
	}
	
	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	int32 GetCurrentDay() const
	{
		return CurrentDay;
	}
	
	/*
	낮 이상현상 허용 구간인지
	*/
	UFUNCTION(BlueprintPure, Category = "Game|Phase")
	bool IsDayAnomalyAllowed() const;
	
	UPROPERTY(BlueprintAssignable, Category = "Game|Phase")
	FHotelPhaseChanged OnPhaseChanged;
	UPROPERTY(BlueprintAssignable, Category = "Game|Phase")
	FDaySlotChanged OnDaySlotChanged;
	UPROPERTY(BlueprintAssignable, Category = "Game|Phase")
	FHotelDayEvent OnDayStarted;
	UPROPERTY(BlueprintAssignable, Category = "Game|Phase")
	FHotelDayEvent OnRunCompleted;
	
private:
	UPROPERTY()
	TObjectPtr<UGameClockSubsystem> Clock;
	
	UFUNCTION()
	void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);
	void StartDay();
	void EnterSettlement();
	void SetPhase(EHotelPhase NewPhase);
	void SetDaySlot(EDaySlot NewSlot);

	EHotelPhase EvaluatePhase(int32 Hour) const;
	EDaySlot EvaluateDaySlot(int32 Hour) const;

	EHotelPhase CurrentPhase = EHotelPhase::Day;
	EDaySlot CurrentDaySlot = EDaySlot::None;
	int32 CurrentDay = 1;
	
	bool bRunActive = false;
};