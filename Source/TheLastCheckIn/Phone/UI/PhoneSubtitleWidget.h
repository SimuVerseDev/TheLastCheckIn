#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PhoneSubtitleWidget.generated.h"

class UTextBlock;

UCLASS()
class THELASTCHECKIN_API UPhoneSubtitleWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetSubtitle(const FText& Text);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
};