#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Night/Settlement/DailySettlementTypes.h"
#include "SettlementWidget.generated.h"

class UButton;
class UTextBlock;
DECLARE_MULTICAST_DELEGATE(FSettlementConfirmed);

// Provides a native layout; a Widget Blueprint may optionally replace it.
UCLASS()
class THELASTCHECKIN_API USettlementWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetResult(const FDailySettlementData& InResult, bool bLastDay);
	FSettlementConfirmed OnConfirmed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> NextDayButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DayText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> IncomeText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CapitalText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ReputationText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AnomaliesText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(BlueprintReadOnly, Category="Night|Settlement") FDailySettlementData Result;

private:
	UFUNCTION() void HandleConfirm();
	void Refresh();
	void BuildDefaultLayout();
	bool bIsLastDay = false;
	bool bConfirmed = false;
};
