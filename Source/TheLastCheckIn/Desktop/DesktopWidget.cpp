#include "Desktop/DesktopWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"

void UDesktopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	APlayerController* PC = GetOwningPlayer();
	if (!PC || bInputCaptured) return;
	InputOwner = PC;
	bInputCaptured = true;
	bPreviousCursor = PC->bShowMouseCursor;
	if (GEngine)
	{
		bPreviousDebugMessages = GEngine->bEnableOnScreenDebugMessagesDisplay;
		GEngine->bEnableOnScreenDebugMessagesDisplay = false;
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (APawn* Pawn = PC->GetPawn())
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
	}
	SetIsFocusable(true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void UDesktopWidget::NativeDestruct()
{
	ReleaseDesktopInput();
	Super::NativeDestruct();
}

void UDesktopWidget::ReleaseDesktopInput()
{
	if (bInputCaptured)
	{
		if (APlayerController* PC = InputOwner.Get())
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = bPreviousCursor;
		}
		bInputCaptured = false;
		if (GEngine) GEngine->bEnableOnScreenDebugMessagesDisplay = bPreviousDebugMessages;
		InputOwner.Reset();
	}
}

void UDesktopWidget::CloseDesktop()
{
	// Release synchronously: Slate may retain the clicked widget until the event finishes.
	ReleaseDesktopInput();
	RemoveFromParent();
}

FReply UDesktopWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDesktop();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
