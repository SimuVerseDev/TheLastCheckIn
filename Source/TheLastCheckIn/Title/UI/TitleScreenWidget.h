#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleScreenWidget.generated.h"

class UTitleMenuButton;

// WBP_Title 부모: 메뉴 버튼 동작 처리
UCLASS()
class THELASTCHECKIN_API UTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTitleMenuButton> Menu_NewGame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTitleMenuButton> Menu_Quit;

	// NEW GAME 시 열 맵
	UPROPERTY(EditAnywhere, Category = "Title")
	TSoftObjectPtr<UWorld> GameLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Design/Maps/Level_combine.Level_combine")));

private:
	UFUNCTION() void HandleNewGame();
	UFUNCTION() void HandleQuit();
};