#include "Core/Reputation/UI/ReputationHUDWidget.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "ReputationHUD"

void UReputationHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
}

void UReputationHUDWidget::BuildDefaultLayout()
{
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(FLinearColor::Transparent);
	Root->SetHorizontalAlignment(HAlign_Right);
	Root->SetVerticalAlignment(VAlign_Top);
	Root->SetPadding(FMargin(24.0f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->SetContent(Stack);

	StarText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StarText"));
	StarText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	Stack->AddChildToVerticalBox(StarText)->SetHorizontalAlignment(HAlign_Right);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Stack->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	auto AddBox = [this, Row](const FText& Label, const FName ValueName, float Width)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->AddChildToHorizontalBox(Column)->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
		Title->SetText(Label);
		Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 13));
		Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(Width);
		Column->AddChildToVerticalBox(Size);

		UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
		Box->SetBrushColor(FLinearColor(0.95f, 0.95f, 0.93f, 0.92f));
		Box->SetPadding(FMargin(8.0f, 2.0f));
		Box->SetHorizontalAlignment(HAlign_Right);
		Size->SetContent(Box);

		UTextBlock* Value = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), ValueName);
		Value->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
		Value->SetColorAndOpacity(FSlateColor(FLinearColor(0.08f, 0.06f, 0.05f)));
		Box->SetContent(Value);
		return Value;
	};
	CapitalText = AddBox(LOCTEXT("CapitalLabel", "소지금"), TEXT("CapitalText"), 170.0f);
	ReputationText = AddBox(LOCTEXT("ReputationLabel", "평판"), TEXT("ReputationText"), 100.0f);
}

void UReputationHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UWorld* World = GetWorld();
	Reputation = World ? World->GetSubsystem<UHotelReputationSubsystem>() : nullptr;
	Economy = World ? World->GetSubsystem<UHotelEconomySubsystem>() : nullptr;
	if (Reputation)
	{
		Reputation->OnReputationChanged.AddUniqueDynamic(this, &ThisClass::HandleReputationChanged);
		Reputation->OnReputationDepleted.AddUniqueDynamic(this, &ThisClass::HandleReputationDepleted);
	}
	if (Economy)
	{
		Economy->OnCapitalChanged.AddUniqueDynamic(this, &ThisClass::HandleCapitalChanged);
	}
	Refresh();
}

void UReputationHUDWidget::NativeDestruct()
{
	if (Reputation)
	{
		Reputation->OnReputationChanged.RemoveDynamic(this, &ThisClass::HandleReputationChanged);
		Reputation->OnReputationDepleted.RemoveDynamic(this, &ThisClass::HandleReputationDepleted);
	}
	if (Economy)
	{
		Economy->OnCapitalChanged.RemoveDynamic(this, &ThisClass::HandleCapitalChanged);
	}
	Super::NativeDestruct();
}

void UReputationHUDWidget::HandleReputationChanged(int32 OldValue, int32 NewValue, EReputationReason Reason)
{
	Refresh();
}

void UReputationHUDWidget::HandleReputationDepleted()
{
	Refresh();
}

void UReputationHUDWidget::HandleCapitalChanged(float NewCapital)
{
	Refresh();
}

void UReputationHUDWidget::Refresh()
{
	if (Reputation)
	{
		if (ReputationText)
		{
			ReputationText->SetText(FText::AsNumber(Reputation->GetReputation()));
		}
		if (StarText)
		{
			StarText->SetText(Reputation->IsFired()
				? LOCTEXT("Fired", "해고됨")
				: FText::Format(LOCTEXT("Star", "{0}성 호텔"), FText::AsNumber(Reputation->GetStarLevel())));
		}
	}
	if (Economy && CapitalText)
	{
		CapitalText->SetText(FText::AsNumber(FMath::RoundToInt64(Economy->GetCapital())));
	}
}

#undef LOCTEXT_NAMESPACE
