// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tutorial/TutorialSubsystem.h"
#include "TutorialQuestWidget.generated.h"

class UTextBlock;
class UVerticalBox;

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
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Objective;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> VerticalBox_RoomList;
private:
	UFUNCTION() void HandleStepChanged(ETutorialStep OldStep, ETutorialStep NewStep);
	UPROPERTY(Transient) TObjectPtr<UTutorialSubsystem> Tutorial;
};
