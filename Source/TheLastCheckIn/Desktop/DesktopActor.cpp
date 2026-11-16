#include "Desktop/DesktopActor.h"
#include "Desktop/DesktopWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool ADesktopActor::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return PC && PC->IsLocalController() && DesktopWidgetClass
		&& (!OpenWidget || !OpenWidget->IsInViewport());
}

FText ADesktopActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("Desktop", "OpenDesktop", "컴퓨터 사용");
}

void ADesktopActor::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor)) return;
	APlayerController* PC = Cast<APlayerController>(CastChecked<APawn>(Interactor)->GetController());
	OpenWidget = CreateWidget<UDesktopWidget>(PC, DesktopWidgetClass);
	// Use the viewport layer so viewport HUDs (including tutorial quests) stay behind it.
	if (OpenWidget) OpenWidget->AddToViewport(9000);
}

void ADesktopActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OpenWidget) OpenWidget->RemoveFromParent();
	OpenWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}
