#include "Phone/UI/PhoneSubtitleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UPhoneSubtitleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Box->SetPadding(FMargin(20.0f, 8.0f));
	
	UCanvasPanelSlot* BoxSlot = Canvas->AddChildToCanvas(Box);
	BoxSlot->SetAnchors(FAnchors(0.5f, 0.85f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BoxSlot->SetPosition(FVector2D::ZeroVector);
	BoxSlot->SetAutoSize(true);

	Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 20));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetJustification(ETextJustify::Center);
	Box->SetContent(Label);
}

void UPhoneSubtitleWidget::SetSubtitle(const FText& Text)
{
	if (Label)
	{
		Label->SetText(Text);
	}
}