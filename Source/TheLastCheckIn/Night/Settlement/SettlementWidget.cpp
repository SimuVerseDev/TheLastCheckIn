#include "Night/Settlement/SettlementWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "NightSettlement"

void USettlementWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
}

void USettlementWidget::BuildDefaultLayout()
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.03f, 0.04f, 1.0f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	Background->SetPadding(FMargin(32.0f));
	WidgetTree->RootWidget = Background;

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(640.0f);
	Background->SetContent(PanelSize);
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
	PanelSize->SetContent(Panel);

	auto AddText = [this, Panel](const FName Name, const FText& Text, int32 FontSize)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(Text);
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
		Label->SetAutoWrapText(true);
		Panel->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 6.0f));
		return Label;
	};
	DayText = AddText(TEXT("DayText"), FText::GetEmpty(), 32);
	AddText(TEXT("IncomeLabel"), LOCTEXT("IncomeLabel", "오늘 수입"), 18);
	IncomeText = AddText(TEXT("IncomeText"), FText::GetEmpty(), 24);
	AddText(TEXT("CapitalLabel"), LOCTEXT("CapitalLabel", "보유 금액"), 18);
	CapitalText = AddText(TEXT("CapitalText"), FText::GetEmpty(), 24);
	AddText(TEXT("ReputationLabel"), LOCTEXT("ReputationLabel", "평판"), 18);
	ReputationText = AddText(TEXT("ReputationText"), FText::GetEmpty(), 22);
	AddText(TEXT("AnomalyLabel"), LOCTEXT("AnomalyLabel", "이상현상 해결 결과"), 18);

	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
	ListSize->SetHeightOverride(150.0f);
	Panel->AddChildToVerticalBox(ListSize)->SetPadding(FMargin(0.0f, 8.0f));
	UScrollBox* List = WidgetTree->ConstructWidget<UScrollBox>();
	ListSize->SetContent(List);
	AnomaliesText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AnomaliesText"));
	AnomaliesText->SetAutoWrapText(true);
	AnomaliesText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
	List->AddChild(AnomaliesText);

	NextDayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NextDayButton"));
	NextDayButton->SetBackgroundColor(FLinearColor(0.22f, 0.35f, 0.48f, 1.0f));
	Panel->AddChildToVerticalBox(NextDayButton)->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
	ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmText"));
	ConfirmText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 24));
	ConfirmText->SetJustification(ETextJustify::Center);
	NextDayButton->SetContent(ConfirmText);
}

void USettlementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (NextDayButton) NextDayButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirm);
	Refresh();
}

void USettlementWidget::NativeDestruct()
{
	if (NextDayButton) NextDayButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirm);
	Super::NativeDestruct();
}

void USettlementWidget::SetResult(const FDailySettlementData& InResult, bool bLastDay)
{
	Result = InResult;
	bIsLastDay = bLastDay;
	bConfirmed = false;
	Refresh();
}

void USettlementWidget::HandleConfirm()
{
	if (bConfirmed || !OnConfirmed.IsBound()) return;
	bConfirmed = true;
	if (NextDayButton) NextDayButton->SetIsEnabled(false);
	OnConfirmed.Broadcast();
}

void USettlementWidget::Refresh()
{
	if (DayText) DayText->SetText(FText::Format(LOCTEXT("Day", "{0}일차 정산"), FText::AsNumber(Result.Day)));
	if (IncomeText) IncomeText->SetText(FText::AsNumber(Result.Income));
	if (CapitalText) CapitalText->SetText(FText::AsNumber(Result.Capital));
	if (ReputationText)
	{
		ReputationText->SetText(Result.bHasReputation
			? FText::Format(LOCTEXT("Reputation", "{0} → {1} (변동: {2})"),
				FText::AsNumber(Result.ReputationBefore), FText::AsNumber(Result.ReputationAfter),
				FText::AsNumber(Result.ReputationAfter - Result.ReputationBefore))
			: LOCTEXT("NoReputation", "평판 결과 미등록"));
	}
	if (AnomaliesText)
	{
		TArray<FString> Lines;
		for (const FDailyAnomalyResult& Item : Result.Anomalies)
		{
			FText Status;
			switch (Item.Status)
			{
			case EDailyAnomalyStatus::Resolved: Status = LOCTEXT("Resolved", "해결 완료"); break;
			case EDailyAnomalyStatus::Unresolved: Status = LOCTEXT("Unresolved", "미해결"); break;
			default: Status = LOCTEXT("NotOccurred", "발생하지 않음"); break;
			}
			Lines.Add(FText::Format(LOCTEXT("Anomaly", "{0}: {1}"), Item.DisplayName, Status).ToString());
		}
		AnomaliesText->SetText(Lines.IsEmpty() ? LOCTEXT("NoAnomalies", "이상현상 기록 없음")
			: FText::FromString(FString::Join(Lines, TEXT("\n"))));
	}
	if (ConfirmText) ConfirmText->SetText(bIsLastDay ? LOCTEXT("Finish", "결과 확인") : LOCTEXT("NextDay", "다음 날 시작"));
	if (NextDayButton) NextDayButton->SetIsEnabled(!bConfirmed);
}

#undef LOCTEXT_NAMESPACE
