#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/Reputation/Enums/ReputationTypes.h"
#include "ReputationHUDWidget.generated.h"

class UTextBlock;
class UHotelReputationSubsystem;
class UHotelEconomySubsystem;
class UGameClockSubsystem;

// Always-on reputation/capital HUD. Provides a native layout; a Widget Blueprint may optionally replace it.
UCLASS()
class THELASTCHECKIN_API UReputationHUDWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ReputationText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StarText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CapitalText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DayText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TimeText;

private:
	UFUNCTION() void HandleReputationChanged(int32 OldValue, int32 NewValue, EReputationReason Reason);
	UFUNCTION() void HandleReputationDepleted();
	UFUNCTION() void HandleCapitalChanged(float NewCapital);
	UFUNCTION() void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);
	void Refresh();
	void RefreshClock();
	void BuildDefaultLayout();

	UPROPERTY() TObjectPtr<UHotelReputationSubsystem> Reputation;
	UPROPERTY() TObjectPtr<UHotelEconomySubsystem> Economy;
	UPROPERTY() TObjectPtr<UGameClockSubsystem> Clock;
};
