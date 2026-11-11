#include "Core/Reputation/UI/ReputationHUDWidget.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Core/Reputation/ReputationSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
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
	Root->SetHorizontalAlignment(HAlign_Left);
	Root->SetVerticalAlignment(VAlign_Top);
	Root->SetPadding(FMargin(24.0f));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Panel->SetPadding(FMargin(14.0f, 10.0f));
	Root->SetContent(Panel);

	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Lines);

	auto AddText = [this, Lines](const FName Name, int32 FontSize)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
		Lines->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 2.0f));
		return Label;
	};
	ReputationText = AddText(TEXT("ReputationText"), 20);
	StarText = AddText(TEXT("StarText"), 16);
}

void UReputationHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Reputation = GetWorld() ? GetWorld()->GetSubsystem<UHotelReputationSubsystem>() : nullptr;
	if (Reputation)
	{
		Reputation->OnReputationChanged.AddUniqueDynamic(this, &ThisClass::HandleReputationChanged);
		Reputation->OnReputationDepleted.AddUniqueDynamic(this, &ThisClass::HandleReputationDepleted);
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

void UReputationHUDWidget::Refresh()
{
	if (!Reputation) return;
	if (ReputationText)
	{
		ReputationText->SetText(FText::Format(LOCTEXT("Reputation", "평판 {0} / {1}"),
			FText::AsNumber(Reputation->GetReputation()),
			FText::AsNumber(UReputationSettings::Get()->MaxReputation)));
	}
	if (StarText)
	{
		StarText->SetText(Reputation->IsFired()
			? LOCTEXT("Fired", "해고됨")
			: FText::Format(LOCTEXT("Star", "{0}성 호텔 · 요금 x{1}"),
				FText::AsNumber(Reputation->GetStarLevel()),
				FText::AsNumber(Reputation->GetRoomRateMultiplier())));
	}
}

#undef LOCTEXT_NAMESPACE
