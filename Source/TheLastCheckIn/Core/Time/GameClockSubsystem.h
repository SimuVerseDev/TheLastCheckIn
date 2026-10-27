#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "GameClockSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FGameTimeAdvanced,
	double, PreviousSeconds,
	double, CurrentSeconds);

UCLASS(BlueprintType)
class THELASTCHECKIN_API UGameClockSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	
	//1일차부터 시작
	UFUNCTION(BlueprintCallable, Category = "Game|Time")
	void StartClock(
		double InitialGameSeconds = 21600.0,
		double InTimeRate = 20.0);
	
	UFUNCTION(BlueprintCallable, Category = "Game|Time")
	void StopClock();
	
	UFUNCTION(BlueprintCallable, Category = "Game|Time")
	void ResumeClock();
	
	UFUNCTION(BlueprintCallable, Category = "Game|Time")
	double GetTotalGameSeconds() const
	{
		return TotalGameSeconds;
	}
	
	UFUNCTION(BlueprintPure, Category = "Game|Time")
	int32 GetDay() const;

	UFUNCTION(BlueprintPure, Category = "Game|Time")
	int32 GetHour() const;

	UFUNCTION(BlueprintPure, Category = "Game|Time")
	int32 GetMinute() const;

	UFUNCTION(BlueprintPure, Category = "Game|Time")
	bool IsClockRunning() const
	{
		return bRunning;
	}

	UPROPERTY(BlueprintAssignable, Category = "Game|Time")
	FGameTimeAdvanced OnTimeAdvanced;

	virtual void Deinitialize() override;

private:
	void UpdateClock();

	FTimerHandle ClockTimerHandle;

	double TotalGameSeconds = 0.0;
	double LastWorldSeconds = 0.0;

	// 월드 1초 동안 진행하는 게임 내부 시간(초)
	double TimeRate = 20.0;

	bool bRunning = false;

	static constexpr float UpdateInterval = 0.25f;
	
	
};
