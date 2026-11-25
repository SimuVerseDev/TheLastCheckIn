// Fill out your copyright notice in the Description page of Project Settings.


#include "Tutorial/UI/TutorialQuestWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Core/Reputation/UI/ReputationHUDSubsystem.h"

void UTutorialQuestWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Tutorial = GetWorld() ? GetWorld()->GetSubsystem<UTutorialSubsystem>() : nullptr;
	if (Tutorial)
	{
		Tutorial->OnStepChanged.AddUniqueDynamic(this, &ThisClass::HandleStepChanged);
		Tutorial->OnProgressChanged.AddUniqueDynamic(this, &ThisClass::RefreshQuest);
	}
	HUDSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UReputationHUDSubsystem>() : nullptr;
	if (HUDSubsystem)
	{
		HUDSubsystem->OnHUDVisibilityChanged.AddUObject(this, &ThisClass::HandleHUDVisibilityChanged);
	}
	FadeAlpha = 0.0f;
	SetRenderOpacity(0.0f);
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
	if (HUDSubsystem)
	{
		HUDSubsystem->OnHUDVisibilityChanged.RemoveAll(this);
	}
	HUDSubsystem = nullptr;
	Super::NativeDestruct();
}

void UTutorialQuestWidget::HandleHUDVisibilityChanged(bool bVisible)
{
	RefreshQuest();
}

void UTutorialQuestWidget::HandleStepChanged(ETutorialStep OldStep, ETutorialStep NewStep)
{
	FadeAlpha = 0.0f;
	SetRenderOpacity(0.0f);
	RefreshQuest();
}

void UTutorialQuestWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (FadeAlpha < 1.0f)
	{
		FadeAlpha = FMath::Min(1.0f, FadeAlpha + InDeltaTime / FMath::Max(FadeInTime, KINDA_SMALL_NUMBER));
		SetRenderOpacity(FadeAlpha);
	}
}

void UTutorialQuestWidget::RefreshQuest()
{
	if (IsDesignTime()) return;
	const bool bActive = Tutorial && Tutorial->IsTutorialActive();
	const bool bCompletionNotice = Tutorial && Tutorial->ShouldShowCompletionNotice();
	const bool bHUDVisible = !HUDSubsystem || HUDSubsystem->IsHUDVisible();
	SetVisibility((bActive || bCompletionNotice) && bHUDVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bActive && !bCompletionNotice) return;

	constexpr int32 TotalSteps = 5;
	int32 Index = TotalSteps;
	int32 Count = -1;
	int32 Goal = 0;
	FText Title;
	FText Desc;
	FText ListHeader;
	FText Footer;

	switch (Tutorial->GetCurrentStep())
	{
	case ETutorialStep::CleanRoom:
		Index = 1;
		Title = NSLOCTEXT("Tutorial", "TitleClean", "객실 정비");
		Desc = NSLOCTEXT("Tutorial", "DescClean", "2층 객실을 점검하고 청소하십시오.");
		ListHeader = INVTEXT("ROOM INSPECTION");
		Count = Tutorial->GetCleanedRoomCount();
		Goal = 4;
		Footer = NSLOCTEXT("Tutorial", "FooterClean", "※ 업무 완료 후 프런트로 복귀하십시오.");
		break;
	case ETutorialStep::UnlockRoom205:
		Index = 2;
		Title = NSLOCTEXT("Tutorial", "TitleUnlock", "객실 개방");
		Desc = NSLOCTEXT("Tutorial", "DescUnlock", "프런트 컴퓨터에서 205호를 개방하십시오.\n관리 → 해금목록 → 구매");
		break;
	case ETutorialStep::CheckInGuest:
		Index = 3;
		Title = NSLOCTEXT("Tutorial", "TitleCheckIn", "투숙객 응대");
		Desc = NSLOCTEXT("Tutorial", "DescCheckIn", "도착한 손님의 체크인을 처리하십시오.");
		ListHeader = INVTEXT("CHECK-IN");
		Count = Tutorial->GetCheckedInTeamCount();
		Goal = 5;
		break;
	case ETutorialStep::ReadBoardMemo:
		Index = 4;
		Title = NSLOCTEXT("Tutorial", "TitleMemo", "전달 사항");
		Desc = NSLOCTEXT("Tutorial", "DescMemo", "게시판에 남겨진 메모를 확인하십시오.");
		break;
	case ETutorialStep::ViewCCTV:
		Index = 5;
		Title = NSLOCTEXT("Tutorial", "TitleCCTV", "야간 감시");
		Desc = NSLOCTEXT("Tutorial", "DescCCTV", "CCTV를 연속으로 1시간 지켜보십시오.");
		ListHeader = INVTEXT("SURVEILLANCE (MIN)");
		Count = FMath::FloorToInt(Tutorial->GetCCTVViewingMinutes());
		Goal = 60;
		break;
	default:
		Title = NSLOCTEXT("Tutorial", "TitleDone", "업무 종료");
		Desc = NSLOCTEXT("Tutorial", "DescDone", "자정 이후 침대에서 잠을 자면\n다음 날로 넘어갑니다.");
		break;
	}

	if (Text_StepIndex) Text_StepIndex->SetText(FText::FromString(FString::Printf(TEXT("%02d / %02d"), Index, TotalSteps)));
	if (Text_Title) Text_Title->SetText(Title);
	if (Text_Objective) Text_Objective->SetText(Desc);
	if (HB_Progress) HB_Progress->SetVisibility(Count >= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Text_ListHeader) Text_ListHeader->SetText(ListHeader);
	if (Text_ListCount) Text_ListCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::Min(Count, Goal), Goal)));
	if (Text_Footer)
	{
		Text_Footer->SetText(Footer);
		Text_Footer->SetVisibility(Footer.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (VerticalBox_RoomList)
		VerticalBox_RoomList->SetVisibility(Tutorial->GetCurrentStep() == ETutorialStep::CleanRoom
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	for (int32 RoomNumber = 201; RoomNumber <= 204; ++RoomNumber)
	{
		const bool bCleaned = Tutorial->IsRoomCleaningCompleted(RoomNumber);
		if (UImage* Check = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("Check_%d"), RoomNumber)))))
		{
			Check->SetRenderOpacity(bCleaned ? 1.0f : 0.0f);
			Check->SetVisibility(bCleaned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (UTextBlock* RoomText = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("Text_Room%d"), RoomNumber)))))
		{
			RoomText->SetRenderOpacity(bCleaned ? 0.45f : 1.0f);
		}
	}
}

