#include "Desktop/DesktopWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UDesktopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	APlayerController* PC = GetOwningPlayer();
	if (!PC || bInputCaptured) return;
	InputOwner = PC;
	bInputCaptured = true;
	bPreviousCursor = PC->bShowMouseCursor;
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
		InputOwner.Reset();
	}
	Super::NativeDestruct();
}

void UDesktopWidget::CloseDesktop()
{
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
