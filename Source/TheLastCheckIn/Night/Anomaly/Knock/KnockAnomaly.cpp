#include "Night/Anomaly/Knock/KnockAnomaly.h"
#include "Night/Anomaly/Knock/KnockDoorActor.h"
#include "Night/Anomaly/UI/AnomalyNoticeWidget.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "CCTV/CCTVMonitor.h"
#include "Phone/HotelPhoneActor.h"
#include "Rooms/RoomActor.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"

AKnockAnomaly::AKnockAnomaly()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AnomalyId = FName("Knock");
	DisplayName = NSLOCTEXT("Anomaly", "KnockName", "노크 소리");
	ComplaintMessages = {
		NSLOCTEXT("Anomaly", "KnockComplaint1", "{Room}호실인데요. 문 밖에서 노크소리가 들리는데 밖엔 아무도 없어요. 누가 장난치는거같은데 한번 봐주실래요?"),
		NSLOCTEXT("Anomaly", "KnockComplaint2", "{Room}호실인데요. 문 밖에서 노크소리가 들리는데 밖엔 아무도 없어요. 무서워요. 얼른 해결해주세요.")
	};

	EarliestStartHour = 20;
	LatestStartHour = 4;
	EscalateAfterMinutes = 24 * 60; // 악화 단계 없음
}

bool AKnockAnomaly::StartAnomaly()
{
	TArray<AKnockDoorActor*> Candidates;
	for (AKnockDoorActor* Door : Doors)
	{
		if (!Door)
		{
			continue;
		}
		const bool bStaffDoor = Door->CanOpen() && !Door->IsOpen();
		const bool bGuestDoor = !Door->CanOpen() && Door->GetRoom()
			&& Door->GetRoom()->GetRoomState() == ERoomState::Occupied;
		if (bStaffDoor || bGuestDoor)
		{
			Candidates.Add(Door);
		}
	}
	if (Candidates.IsEmpty() || !Super::StartAnomaly())
	{
		return false;
	}

	ActiveDoor = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	ActiveDoor->SetAwaitingHorseshoe(true);
	ActiveDoor->OnHorseshoePlaced.AddUniqueDynamic(this, &ThisClass::HandleHorseshoePlaced);

	if (ActiveDoor->CanOpen())
	{
		// CCTV실·직원실: 노크 반복
		GetWorldTimerManager().SetTimer(KnockTimer, this, &ThisClass::TickKnock, KnockInterval, true, 0.0f);
	}
	else if (!ComplaintMessages.IsEmpty())
	{
		const FText RoomText = FText::AsNumber(ActiveDoor->GetRoom()->GetRoomNumber(), &FNumberFormattingOptions::DefaultNoGrouping());
		const FText Message = FText::FormatNamed(ComplaintMessages[ComplaintIndex], TEXT("Room"), RoomText);
		ComplaintIndex = (ComplaintIndex + 1) % ComplaintMessages.Num();

		if (Phone)
		{
			Phone->StartRinging(Message);
		}
		else
		{
			UAnomalyNoticeWidget::Show(this, Message, 5.0f);
		}
	}
	return true;
}

void AKnockAnomaly::TickKnock()
{
	if (!ActiveDoor)
	{
		return;
	}
	/*
	첫 노크는 CCTV를 보고 있을 때만, 그 뒤로는 해결할 때까지 계속 (문이 열려 있으면 멈춤)
	*/
	if (!bKnockRevealed && CCTVMonitor && CCTVMonitor->IsViewingCCTV())
	{
		bKnockRevealed = true;
	}
	if (bKnockRevealed && !ActiveDoor->IsOpen())
	{
		ActiveDoor->PlayKnock();
	}
	else
	{
		ActiveDoor->StopKnock();
	}
}

void AKnockAnomaly::HandleHorseshoePlaced(AKnockDoorActor* Door)
{
	if (Door != ActiveDoor || !IsRunning())
	{
		return;
	}
	if (Phone && !Door->CanOpen())
	{
		Phone->StopRinging();
	}
	ClearActiveDoor();
	Resolve();
}

bool AKnockAnomaly::Fail()
{
	const bool bUnsolved = ActiveDoor != nullptr;
	if (Phone && ActiveDoor && !ActiveDoor->CanOpen())
	{
		Phone->StopRinging();
	}
	ClearActiveDoor();

	if (!Super::Fail())
	{
		return false;
	}
	if (bUnsolved)
	{
		if (UHotelReputationSubsystem* Reputation = GetWorld()->GetSubsystem<UHotelReputationSubsystem>())
		{
			Reputation->AddReputation(EReputationReason::Complaint);
		}
	}
	return true;
}

void AKnockAnomaly::ClearActiveDoor()
{
	GetWorldTimerManager().ClearTimer(KnockTimer);
	bKnockRevealed = false;
	if (ActiveDoor)
	{
		ActiveDoor->StopKnock();
		ActiveDoor->SetAwaitingHorseshoe(false);
		ActiveDoor->OnHorseshoePlaced.RemoveAll(this);
		ActiveDoor = nullptr;
	}
}

void AKnockAnomaly::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearActiveDoor();
	Super::EndPlay(EndPlayReason);
}