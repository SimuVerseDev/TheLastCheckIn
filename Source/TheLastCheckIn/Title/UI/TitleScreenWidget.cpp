#include "Title/UI/TitleScreenWidget.h"

#include "Title/UI/TitleMenuButton.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UTitleScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Menu_NewGame)
	{
		Menu_NewGame->OnMenuClicked.AddDynamic(this, &ThisClass::HandleNewGame);
	}
	if (Menu_Quit)
	{
		Menu_Quit->OnMenuClicked.AddDynamic(this, &ThisClass::HandleQuit);
	}
}

void UTitleScreenWidget::HandleNewGame()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, GameLevel);
}

void UTitleScreenWidget::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}