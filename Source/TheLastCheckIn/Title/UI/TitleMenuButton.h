#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleMenuButton.generated.h"

class UButton;
class UTextBlock;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTitleMenuClicked);

/*
호버 시 ▶ 표시 + 글씨 빛남 + 소리
*/
UCLASS()
class THELASTCHECKIN_API UTitleMenuButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Title")
	FOnTitleMenuClicked OnMenuClicked;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> MenuButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Label;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Arrow;

	UPROPERTY(EditAnywhere, Category = "Title") FText LabelText = FText::FromString(TEXT("MENU"));
	UPROPERTY(EditAnywhere, Category = "Title") FLinearColor NormalColor = FLinearColor(0.82f, 0.76f, 0.66f);
	UPROPERTY(EditAnywhere, Category = "Title") FLinearColor HoverColor = FLinearColor(1.0f, 0.93f, 0.78f);
	UPROPERTY(EditAnywhere, Category = "Title") FLinearColor DisabledColor = FLinearColor(0.4f, 0.38f, 0.35f);
	UPROPERTY(EditAnywhere, Category = "Title") FLinearColor GlowColor = FLinearColor(1.0f, 0.6f, 0.25f, 0.45f);
	UPROPERTY(EditAnywhere, Category = "Title") TObjectPtr<USoundBase> HoverSound;
	UPROPERTY(EditAnywhere, Category = "Title") TObjectPtr<USoundBase> ClickSound;

private:
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
	UFUNCTION() void HandleClicked();

	void SetHighlighted(bool bOn);
};