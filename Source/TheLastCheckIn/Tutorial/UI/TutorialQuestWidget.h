// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tutorial/TutorialSubsystem.h"
#include "TutorialQuestWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UReputationHUDSubsystem;

// Presentation only. Creation and viewport placement are owned by the caller.
UCLASS()
class THELASTCHECKIN_API UTutorialQuestWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Tutorial")
	void RefreshQuest();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Objective;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> VerticalBox_RoomList;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_StepIndex;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ListHeader;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ListCount;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Footer;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> HB_Progress;
	UPROPERTY(EditAnywhere, Category="Tutorial") float FadeInTime = 0.8f;
private:
	UFUNCTION() void HandleStepChanged(ETutorialStep OldStep, ETutorialStep NewStep);
	void HandleHUDVisibilityChanged(bool bVisible);
	UPROPERTY(Transient) TObjectPtr<UTutorialSubsystem> Tutorial;
	UPROPERTY(Transient) TObjectPtr<UReputationHUDSubsystem> HUDSubsystem;
	float FadeAlpha = 1.0f;
};
