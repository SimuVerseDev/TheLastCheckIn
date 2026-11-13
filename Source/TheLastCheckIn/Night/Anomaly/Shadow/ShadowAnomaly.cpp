#include "Night/Anomaly/Shadow/ShadowAnomaly.h"
#include "Night/Anomaly/Shadow/ShadowFigureActor.h"
#include "Night/Anomaly/UI/AnomalyNoticeWidget.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Phone/HotelPhoneActor.h"
#include "Rooms/RoomActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

// 유니티 빌드에서 다른 cpp의 익명 네임스페이스 상수와 겹치지 않도록 이름 분리
namespace ShadowAnomalyTime
{
	constexpr double SecondsPerHour = 3600.0;
	constexpr double SecondsPerDay = 86400.0;
}

AShadowAnomaly::AShadowAnomaly()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AnomalyId = FName("Shadow");
	DisplayName = NSLOCTEXT("Anomaly", "ShadowName", "검은 형체");
	ComplaintMessage = NSLOCTEXT("Anomaly", "ShadowComplaint", "[전화] {Room}호 손님: 방 문이 안 열려요! 빨리 좀 와 주세요.");

	/*
	22시에 시작해서 형체들은 RevealUntilHour 전까지 각자 따로 나타남
	매니저의 시간 경과 악화는 쓰지 않음 - 문고리 형체가 직접 Escalate 호출
	*/
	EarliestStartHour = 22;
	LatestStartHour = 22;
	EscalateAfterMinutes = 24 * 60;
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
	if (UGameClockSubsystem* Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnTimeAdvanced.AddUniqueDynamic(this, &ThisClass::HandleTimeAdvanced);
	}
}

void AShadowAnomaly::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameClockSubsystem* Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnTimeAdvanced.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool AShadowAnomaly::StartAnomaly()
{
	const UGameClockSubsystem* Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>();
	TArray<AShadowFigureActor*> Candidates;
	for (AShadowFigureActor* Figure : DecorFigures)
	{
		if (Figure)
		{
			Candidates.Add(Figure);
		}
	}
	if (!Clock || Candidates.IsEmpty() || !Super::StartAnomaly())
	{
		return false;
	}

	HideAllFigures();
	PendingReveals.Reset();
	ComplaintFigure = nullptr;

	/*
	지금부터 RevealUntilHour(다음 날 새벽)까지 남은 시간
	*/
	const double Now = Clock->GetTotalGameSeconds();
	const double TimeOfDay = FMath::Fmod(Now, ShadowAnomalyTime::SecondsPerDay);
	double Until = RevealUntilHour * ShadowAnomalyTime::SecondsPerHour;
	if (Until <= TimeOfDay)
	{
		Until += ShadowAnomalyTime::SecondsPerDay;
	}
	const double Window = Until - TimeOfDay;

	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, FMath::RandRange(0, i));
	}
	const int32 Count = DecorFigureCount > 0 ? FMath::Min(DecorFigureCount, Candidates.Num()) : Candidates.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		PendingReveals.Add(Candidates[i], Now + FMath::FRandRange(0.0, Window));
	}

	// 문고리 형체는 확률로 0 또는 1개, 투숙 객실은 나타나는 시점에 고름
	DoorRevealSeconds = FMath::FRand() < DoorFigureChance ? Now + FMath::FRandRange(0.0, Window) : -1.0;
	return true;
}

void AShadowAnomaly::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	if (!IsRunning())
	{
		return;
	}

	TArray<AShadowFigureActor*> Due;
	for (auto It = PendingReveals.CreateIterator(); It; ++It)
	{
		if (CurrentSeconds >= It.Value())
		{
			Due.Add(It.Key());
			It.RemoveCurrent();
		}
	}
	for (AShadowFigureActor* Figure : Due)
	{
		ShowFigure(Figure);
	}

	if (DoorRevealSeconds >= 0.0 && CurrentSeconds >= DoorRevealSeconds)
	{
		DoorRevealSeconds = -1.0;
		Escalate();
		TryResolve();
	}
}

bool AShadowAnomaly::Escalate()
{
	TArray<const FShadowDoorFigure*> Occupied;
	for (const FShadowDoorFigure& Door : DoorFigures)
	{
		if (Door.Figure && Door.Room && Door.Room->GetRoomState() == ERoomState::Occupied)
		{
			Occupied.Add(&Door);
		}
	}
	if (Occupied.IsEmpty() || !Super::Escalate())
	{
		return false;
	}

	const FShadowDoorFigure* Picked = Occupied[FMath::RandRange(0, Occupied.Num() - 1)];
	ShowFigure(Picked->Figure);
	ComplaintFigure = Picked->Figure;

	const FText RoomText = FText::AsNumber(Picked->Room->GetRoomNumber(), &FNumberFormattingOptions::DefaultNoGrouping());
	const FText Complaint = FText::FormatNamed(ComplaintMessage, TEXT("Room"), RoomText);
	if (Phone)
	{
		Phone->StartRinging(Complaint);
	}
	else
	{
		UAnomalyNoticeWidget::Show(this, Complaint, 5.0f);
	}
	return true;
}

/*
밤이 끝날 때까지 못 치움 - 컴플레인 걸린 문고리 형체가 남아 있으면 평판 감소
*/
bool AShadowAnomaly::Fail()
{
	if (Phone)
	{
		Phone->StopRinging();
	}
	
	const bool bComplaintLeft = ComplaintFigure && ActiveFigures.Contains(ComplaintFigure);
	if (!Super::Fail())
	{
		return false;
	}
	if (bComplaintLeft)
	{
		if (UHotelReputationSubsystem* Reputation = GetWorld()->GetSubsystem<UHotelReputationSubsystem>())
		{
			Reputation->AddReputation(EReputationReason::Complaint);
		}
	}
	HideAllFigures();
	PendingReveals.Reset();
	DoorRevealSeconds = -1.0;
	ComplaintFigure = nullptr;
	return true;
}

void AShadowAnomaly::HandleFigureAbsorbed(AShadowFigureActor* Figure)
{
	ActiveFigures.Remove(Figure);
	if (Figure == ComplaintFigure)
	{
		ComplaintFigure = nullptr;
	}
	TryResolve();
}

/*
보이는 형체를 다 치웠고 더 나올 형체도 없을 때만 해결
*/
void AShadowAnomaly::TryResolve()
{
	if (IsRunning() && ActiveFigures.IsEmpty() && PendingReveals.IsEmpty() && DoorRevealSeconds < 0.0)
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
