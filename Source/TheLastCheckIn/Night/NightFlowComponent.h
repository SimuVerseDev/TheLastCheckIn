#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "TimerManager.h"
#include "NightFlowComponent.generated.h"

class APlayerController;
class UInputComponent;
class UPlayerInteractionComponent;
class UHotelPhaseSubsystem;
class UDailySettlementSubsystem;
class USettlementWidget;

// Bound to the existing phase subsystem in BeginPlay; no clock jumps are needed.
DECLARE_DELEGATE(FRequestSleepSettlement);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightRunCompleted, int32, Day);

UCLASS()
class THELASTCHECKIN_API UNightBlackoutWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
};

// Native, non-interactive prompt; no Widget Blueprint is required.
UCLASS()
class THELASTCHECKIN_API UNightSleepPromptWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
};

// The project's PlayerController owns this component automatically.
UCLASS(ClassGroup=(Night), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UNightFlowComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNightFlowComponent();
	static UNightFlowComponent* FindForActor(AActor* Actor);
	// Prompt availability does not require the settlement UI or sleep-entry binding.
	UFUNCTION(BlueprintPure, Category="Night") bool CanShowSleepPrompt() const;
	// Actual sleep still requires all settlement dependencies to be ready.
	UFUNCTION(BlueprintPure, Category="Night") bool CanSleep() const;
	UFUNCTION(BlueprintCallable, Category="Night") bool RequestSleep();

	// Bound automatically; exposed for native integrations using a custom phase handler.
	FRequestSleepSettlement RequestSleepSettlement;
	UPROPERTY(BlueprintAssignable, Category="Night") FNightRunCompleted OnRunCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditAnywhere, Category="Night|UI") TSubclassOf<USettlementWidget> SettlementWidgetClass;
	UPROPERTY(EditAnywhere, Category="Night|Sleep", meta=(ClampMin="0.01", Units="s"))
	float BlackoutSeconds = 0.5f;

private:
	APlayerController* GetPlayerController() const;
	void FinishBlackout();
	void ShowSettlement();
	void ConfirmSettlement();
	void LockInput();
	void UnlockInput();
	void ClearPresentation();
	void SetSleepPromptVisible(bool bVisible);
	UFUNCTION() void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);
	UFUNCTION() void HandleDayStarted(int32 Day);
	UFUNCTION() void HandleRunCompleted(int32 Day);

	UPROPERTY() TObjectPtr<UHotelPhaseSubsystem> Phase;
	UPROPERTY() TObjectPtr<UDailySettlementSubsystem> Settlement;
	UPROPERTY() TObjectPtr<USettlementWidget> SettlementWidget;
	UPROPERTY() TObjectPtr<UNightBlackoutWidget> BlackoutWidget;
	UPROPERTY() TObjectPtr<UNightSleepPromptWidget> SleepPromptWidget;
	UPROPERTY() TObjectPtr<UInputComponent> InputBlocker;
	TWeakObjectPtr<APlayerController> LockedController;
	TWeakObjectPtr<UPlayerInteractionComponent> PausedInteraction;
	FTimerHandle BlackoutTimer;
	bool bBlackout = false;
	bool bInputLocked = false;
	bool bInteractionWasTicking = false;
	bool bPreviousCursor = false;
	bool bRunCompleted = false;
};
