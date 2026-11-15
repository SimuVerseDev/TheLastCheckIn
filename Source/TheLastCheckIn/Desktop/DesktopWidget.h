#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DesktopWidget.generated.h"

/** Base for WBP_Desktop. Owns input only while the desktop is on screen. */
UCLASS()
class THELASTCHECKIN_API UDesktopWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Desktop")
	void CloseDesktop();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<APlayerController> InputOwner;
	bool bInputCaptured = false;
	bool bPreviousCursor = false;
};
