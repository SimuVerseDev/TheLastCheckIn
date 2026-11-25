#include "Title/TitlePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

ATitlePlayerController::ATitlePlayerController()
{
	static ConstructorHelpers::FClassFinder<UUserWidget> TitleWidgetFinder(TEXT("/Game/UI/Title/WBP_Title"));
	if (TitleWidgetFinder.Succeeded())
	{
		TitleWidgetClass = TitleWidgetFinder.Class;
	}
}

void ATitlePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || !TitleWidgetClass)
	{
		return;
	}

	TitleWidget = CreateWidget<UUserWidget>(this, TitleWidgetClass);
	TitleWidget->AddToViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TitleWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}