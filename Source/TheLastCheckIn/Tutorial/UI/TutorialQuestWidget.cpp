// Fill out your copyright notice in the Description page of Project Settings.


#include "Tutorial/UI/TutorialQuestWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Engine/World.h"

void UTutorialQuestWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Tutorial = GetWorld() ? GetWorld()->GetSubsystem<UTutorialSubsystem>() : nullptr;
	if (Tutorial)
	{
		Tutorial->OnStepChanged.AddUniqueDynamic(this, &ThisClass::HandleStepChanged);
		Tutorial->OnProgressChanged.AddUniqueDynamic(this, &ThisClass::RefreshQuest);
	}
	RefreshQuest();
}

void UTutorialQuestWidget::NativeDestruct()
{
	if (Tutorial)
	{
		Tutorial->OnStepChanged.RemoveAll(this);
		Tutorial->OnProgressChanged.RemoveAll(this);
	}
	Tutorial = nullptr;
	Super::NativeDestruct();
}

void UTutorialQuestWidget::HandleStepChanged(ETutorialStep OldStep, ETutorialStep NewStep)
{
	RefreshQuest();
}

void UTutorialQuestWidget::RefreshQuest()
{
	if (IsDesignTime()) return;
	const bool bActive = Tutorial && Tutorial->IsTutorialActive();
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bActive) return;
	if (Text_Title) Text_Title->SetText(NSLOCTEXT("Tutorial", "DayOneTitle", "튜토리얼 1일차"));
	if (Text_Objective) Text_Objective->SetText(Tutorial->GetObjectiveText());
	if (VerticalBox_RoomList)
		VerticalBox_RoomList->SetVisibility(Tutorial->GetCurrentStep() == ETutorialStep::CleanRoom
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	for (int32 RoomNumber = 201; RoomNumber <= 204; ++RoomNumber)
	{
		if (UImage* Check = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("Check_%d"), RoomNumber)))))
		{
			const bool bCleaned = Tutorial->IsRoomCleaningCompleted(RoomNumber);
			Check->SetRenderOpacity(bCleaned ? 1.0f : 0.0f);
			Check->SetVisibility(bCleaned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
}

