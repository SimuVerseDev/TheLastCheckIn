#include "Night/Anomaly/UI/AnomalyNoticeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UAnomalyNoticeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	Box->SetPadding(FMargin(18.0f, 10.0f));

	UCanvasPanelSlot* BoxSlot = Canvas->AddChildToCanvas(Box);
	BoxSlot->SetAnchors(FAnchors(0.5f, 0.15f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BoxSlot->SetPosition(FVector2D::ZeroVector);
	BoxSlot->SetAutoSize(true);

	Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Box->SetContent(Label);
}

void UAnomalyNoticeWidget::SetMessage(const FText& Message)
{
	if (Label)
	{
		Label->SetText(Message);
	}
}

void UAnomalyNoticeWidget::Show(const UObject* WorldContext, const FText& Message, float Seconds)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	UAnomalyNoticeWidget* Widget = CreateWidget<UAnomalyNoticeWidget>(PC, UAnomalyNoticeWidget::StaticClass());
	if (!Widget)
	{
		return;
	}
	Widget->SetMessage(Message);
	Widget->AddToViewport(50);

	TWeakObjectPtr<UAnomalyNoticeWidget> WeakWidget(Widget);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWidget]()
	{
		if (UAnomalyNoticeWidget* Notice = WeakWidget.Get())
		{
			Notice->RemoveFromParent();
		}
	}), Seconds, false);
}