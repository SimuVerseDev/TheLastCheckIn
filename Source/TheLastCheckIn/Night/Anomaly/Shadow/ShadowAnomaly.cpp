#include "Night/Anomaly/Shadow/ShadowAnomaly.h"
#include "Night/Anomaly/Shadow/ShadowFigureActor.h"
#include "Night/Anomaly/UI/AnomalyNoticeWidget.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Rooms/RoomActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AShadowAnomaly::AShadowAnomaly()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AnomalyId = FName("Shadow");
	DisplayName = NSLOCTEXT("Anomaly", "ShadowName", "검은 형체");
	ComplaintMessage = NSLOCTEXT("Anomaly", "ShadowComplaint", "[전화] {Room}호 손님: 방 문이 안 열려요! 빨리 좀 와 주세요.");
	EarliestStartHour = 22;
	LatestStartHour = 4;
}

void AShadowAnomaly::BeginPlay()
{
	Super::BeginPlay();
	for (AShadowFigureActor* Figure : DecorFigures)
	{
		if (Figure)
		{
			Figure->OnAbsorbed.AddUniqueDynamic(this, &ThisClass::HandleFigureAbsorbed);
		}
	}
	for (const FShadowDoorFigure& Door : DoorFigures)
	{
		if (Door.Figure)
		{
			Door.Figure->OnAbsorbed.AddUniqueDynamic(this, &ThisClass::HandleFigureAbsorbed);
		}
	}
}

bool AShadowAnomaly::StartAnomaly()
{
	TArray<AShadowFigureActor*> Candidates;
	for (AShadowFigureActor* Figure : DecorFigures)
	{
		if (Figure)
		{
			Candidates.Add(Figure);
		}
	}
	if (Candidates.IsEmpty() || !Super::StartAnomaly())
	{
		return false;
	}

	HideAllFigures();
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, FMath::RandRange(0, i));
	}
	const int32 Count = DecorFigureCount > 0 ? FMath::Min(DecorFigureCount, Candidates.Num()) : Candidates.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		ShowFigure(Candidates[i]);
	}
	return true;
}

bool AShadowAnomaly::Escalate()
{
	if (!Super::Escalate())
	{
		return false;
	}

	int32 ComplainingRoom = INDEX_NONE;
	for (const FShadowDoorFigure& Door : DoorFigures)
	{
		if (Door.Figure && Door.Room && Door.Room->GetRoomState() == ERoomState::Occupied)
		{
			ShowFigure(Door.Figure);
			if (ComplainingRoom == INDEX_NONE)
			{
				ComplainingRoom = Door.Room->GetRoomNumber();
			}
		}
	}

	// 투숙객이 없으면 컴플레인도 없음
	if (ComplainingRoom != INDEX_NONE)
	{
		const FText RoomText = FText::AsNumber(ComplainingRoom, &FNumberFormattingOptions::DefaultNoGrouping());
		UAnomalyNoticeWidget::Show(this, FText::FormatNamed(ComplaintMessage, TEXT("Room"), RoomText), 5.0f);

		if (UHotelReputationSubsystem* Reputation = GetWorld()->GetSubsystem<UHotelReputationSubsystem>())
		{
			Reputation->AddReputation(EReputationReason::Complaint);
		}
	}
	return true;
}

bool AShadowAnomaly::Fail()
{
	if (!Super::Fail())
	{
		return false;
	}
	HideAllFigures();
	return true;
}

void AShadowAnomaly::HandleFigureAbsorbed(AShadowFigureActor* Figure)
{
	ActiveFigures.Remove(Figure);
	if (ActiveFigures.IsEmpty() && IsRunning())
	{
		Resolve();
	}
}

void AShadowAnomaly::ShowFigure(AShadowFigureActor* Figure)
{
	if (Figure && !ActiveFigures.Contains(Figure))
	{
		Figure->Appear();
		ActiveFigures.Add(Figure);
	}
}

void AShadowAnomaly::HideAllFigures()
{
	for (AShadowFigureActor* Figure : DecorFigures)
	{
		if (Figure)
		{
			Figure->Disappear();
		}
	}
	for (const FShadowDoorFigure& Door : DoorFigures)
	{
		if (Door.Figure)
		{
			Door.Figure->Disappear();
		}
	}
	ActiveFigures.Reset();
}