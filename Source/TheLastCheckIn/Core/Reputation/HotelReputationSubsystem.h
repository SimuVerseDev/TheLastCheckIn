#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/Reputation/Enums/ReputationTypes.h"
#include "HotelReputationSubsystem.generated.h"

class UHotelPhaseSubsystem;
class UDailySettlementSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FReputationChanged,
	int32, OldValue,
	int32, NewValue,
	EReputationReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FStarLevelChanged,
	int32, OldStar,
	int32, NewStar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FReputationDepleted);

UCLASS()
class THELASTCHECKIN_API UHotelReputationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	/*
	call
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Reputation")
	void AddReputation(EReputationReason Reason);
	UFUNCTION(BlueprintCallable, Category = "Game|Reputation")
	void ApplyRoomCleanliness(float Cleanliness);
	UFUNCTION(BlueprintCallable, Category = "Game|Reputation")
	bool TryApplyForcedCheckout();
	
	/*
	get
	*/
	UFUNCTION(BlueprintPure, Category = "Game|Reputation")
	int32 GetReputation() const { return Reputation; }
	UFUNCTION(BlueprintPure, Category = "Game|Reputation")
	int32 GetStarLevel() const;
	UFUNCTION(BlueprintPure, Category = "Game|Reputation")
	float GetRoomRateMultiplier() const;
	UFUNCTION(BlueprintPure, Category = "Game|Reputation")
	bool IsFired() const { return bFired; }
	
	/*
	alarm
	*/
	UPROPERTY(BlueprintAssignable, Category = "Game|Reputation")
	FReputationChanged OnReputationChanged;
	UPROPERTY(BlueprintAssignable, Category = "Game|Reputation")
	FStarLevelChanged OnStarLevelChanged;
	UPROPERTY(BlueprintAssignable, Category = "Game|Reputation")
	FReputationDepleted OnReputationDepleted;
	
private:
	UFUNCTION()
	void HandleDayStarted(int32 Day);
	void ApplyDelta(int32 Delta, EReputationReason Reason);
	UPROPERTY() TObjectPtr<UHotelPhaseSubsystem> Phase;
	UPROPERTY() TObjectPtr<UDailySettlementSubsystem> Settlement;
	int32 Reputation = 0;
	int32 DayStartReputation = 0;
	bool bFired = false;
};