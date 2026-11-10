#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CCTVPadWidget.generated.h"

class UImage;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCCTVPadTransition);

UCLASS(Abstract)
class THELASTCHECKIN_API UCCTVPadWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void PlayPadUp();
	void PlayPadDown();
	
	UPROPERTY(BlueprintAssignable, Category = "CCTV")
	FOnCCTVPadTransition OnPadCovered;
	UPROPERTY(BlueprintAssignable, Category="CCTV")
	FOnCCTVPadTransition OnPadLowered;
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_PadUp;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> Image_Pad;

	/* noise */
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Static;

private:
	UFUNCTION()
	void HandlePadAnimFinished();
	
	bool bGoingUp = false;
	bool bAnimating = false;
};