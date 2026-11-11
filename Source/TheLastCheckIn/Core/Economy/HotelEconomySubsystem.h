#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HotelEconomySubsystem.generated.h"

class UHotelPhaseSubsystem;

UCLASS()
class THELASTCHECKIN_API UHotelEconomySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	UFUNCTION(BlueprintCallable, Category = "Game|Economy")
	void AddIncome(float Amount);

	// Purchases affect capital, not the day's gross lodging income.
	UFUNCTION(BlueprintCallable, Category = "Game|Economy")
	bool TrySpend(float Amount);
	
	UFUNCTION(BlueprintPure, Category = "Game|Economy")
	float GetCapital() const
	{
		return Capital;
	}
	
	UFUNCTION(BlueprintPure, Category = "Game|Economy")
	float GetTodayIncome() const
	{
		return TodayIncome;
	}
private:
	UPROPERTY()
	TObjectPtr<UHotelPhaseSubsystem> Phase;
	
	UFUNCTION()
	void HandleDayStarted(int32 Day);
	
	float Capital = 0.0f;
	float TodayIncome = 0.0f;
};
