#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TitlePlayerController.generated.h"

class UUserWidget;

UCLASS()
class THELASTCHECKIN_API ATitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATitlePlayerController();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Title")
	TSubclassOf<UUserWidget> TitleWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> TitleWidget;
};