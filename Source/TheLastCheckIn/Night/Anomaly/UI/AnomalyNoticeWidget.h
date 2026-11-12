#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AnomalyNoticeWidget.generated.h"

class UTextBlock;

UCLASS()
class THELASTCHECKIN_API UAnomalyNoticeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static void Show(const UObject* WorldContext, const FText& Message, float Seconds = 3.0f);

	void SetMessage(const FText& Message);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
};