#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Anomaly/AnomalyTypes.h"
#include "AnomalyBase.generated.h"

class AAnomalyBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAnomalyStateChanged,
	AAnomalyBase*, Anomaly,
	EAnomalyState, OldState,
	EAnomalyState, NewState);

// Base for every anomaly. Children only decide how it looks and how it is solved.
UCLASS(Abstract)
class THELASTCHECKIN_API AAnomalyBase : public AActor
{
	GENERATED_BODY()
public:
	AAnomalyBase();

	/*
	called by the manager.
	*/
	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	virtual bool StartAnomaly();
	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	virtual bool Escalate();
	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	virtual bool Fail();

	/*
	Called by the child when the player solves it.
	*/
	UFUNCTION(BlueprintCallable, Category="Night|Anomaly")
	virtual bool Resolve();

	UFUNCTION(BlueprintPure, Category="Night|Anomaly")
	EAnomalyState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Night|Anomaly")
	bool IsRunning() const;

	UPROPERTY(BlueprintAssignable, Category="Night|Anomaly")
	FAnomalyStateChanged OnStateChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Anomaly")
	FName AnomalyId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Anomaly")
	FText DisplayName;
	
	/*
	Game minutes after start before it escalates (60 = 1 game hour).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Anomaly", meta=(ClampMin="1"))
	int32 EscalateAfterMinutes = 60;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Anomaly")
	void OnAnomalyStarted();
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Anomaly")
	void OnAnomalyEscalated();
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Anomaly")
	void OnAnomalyResolved();
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Anomaly")
	void OnAnomalyFailed();

private:
	bool TrySetState(EAnomalyState NewState);

	EAnomalyState State = EAnomalyState::Idle;
};