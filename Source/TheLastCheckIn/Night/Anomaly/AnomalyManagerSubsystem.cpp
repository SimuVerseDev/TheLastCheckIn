#include "Night/Anomaly/AnomalyManagerSubsystem.h"
#include "TheLastCheckIn.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "Night/Settlement/DailySettlementSubsystem.h"
#include "Night/Anomaly/AnomalyNightPlan.h"
#include "Night/Omen/OmenBase.h"
#include "Engine/DataTable.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/GameFlow/HotelPhaseSettings.h"
#include "Night/Anomaly/UI/AnomalyNoticeWidget.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "EngineUtils.h"
#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Scare/ScareBase.h"

void UAnomalyManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Clock = Collection.InitializeDependency<UGameClockSubsystem>();
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	Settlement = Collection.InitializeDependency<UDailySettlementSubsystem>();
	Clock->OnTimeAdvanced.AddDynamic(this, &ThisClass::HandleTimeAdvanced);
	Phase->OnPhaseChanged.AddDynamic(this, &ThisClass::HandlePhaseChanged);
	Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
}

void UAnomalyManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (TActorIterator<AAnomalyBase> It(&InWorld); It; ++It)
	{
		RegisterAnomaly(*It);
	}
}

void UAnomalyManagerSubsystem::Deinitialize()
{
	if (Clock)
	{
		Clock->OnTimeAdvanced.RemoveAll(this);
	}
	if (Phase)
	{
		Phase->OnPhaseChanged.RemoveAll(this);
	}
	for (const TWeakObjectPtr<AAnomalyBase>& Weak : Anomalies)
	{
		if (AAnomalyBase* Anomaly = Weak.Get())
		{
			Anomaly->OnStateChanged.RemoveAll(this);
		}
	}
	Anomalies.Reset();
	StartSeconds.Reset();
	PlannedEvents.Reset();
	Super::Deinitialize();
}

void UAnomalyManagerSubsystem::RegisterAnomaly(AAnomalyBase* Anomaly)
{
	if (!Anomaly || Anomalies.Contains(Anomaly))
	{
		return;
	}
	Anomalies.Add(Anomaly);
	Anomaly->OnStateChanged.AddDynamic(this, &ThisClass::HandleAnomalyStateChanged);
}

bool UAnomalyManagerSubsystem::StartAnomalyById(FName AnomalyId)
{
	AAnomalyBase* Anomaly = FindAnomaly(AnomalyId);
	return Anomaly && Anomaly->StartAnomaly();
}

AAnomalyBase* UAnomalyManagerSubsystem::FindAnomaly(FName AnomalyId) const
{
	for (const TWeakObjectPtr<AAnomalyBase>& Weak : Anomalies)
	{
		AAnomalyBase* Anomaly = Weak.Get();
		if (Anomaly && Anomaly->AnomalyId == AnomalyId)
		{
			return Anomaly;
		}
	}
	return nullptr;
}

TArray<AAnomalyBase*> UAnomalyManagerSubsystem::GetRunningAnomalies() const
{
	TArray<AAnomalyBase*> Result;
	for (const TWeakObjectPtr<AAnomalyBase>& Weak : Anomalies)
	{
		AAnomalyBase* Anomaly = Weak.Get();
		if (Anomaly && Anomaly->IsRunning())
		{
			Result.Add(Anomaly);
		}
	}
	return Result;
}

void UAnomalyManagerSubsystem::HandleAnomalyStateChanged(AAnomalyBase* Anomaly, EAnomalyState OldState, EAnomalyState NewState)
{
	if (!Anomaly)
	{
		return;
	}
	if (NewState == EAnomalyState::Active && Clock)
	{
		StartSeconds.Add(Anomaly, Clock->GetTotalGameSeconds());
	}
	else
	{
		StartSeconds.Remove(Anomaly);
	}

	RecordResult(Anomaly);

	if (NewState == EAnomalyState::Resolved)
	{
		UAnomalyNoticeWidget::Show(this,
			FText::Format(
				NSLOCTEXT("Anomaly", "ResolvedNotice", "{0} 해결"),
				Anomaly->DisplayName));
	}

}

void UAnomalyManagerSubsystem::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	/*
	예약 시각이 지난 이벤트 실행 (PlannedEvents는 시각순 정렬 상태)
	*/
	TArray<FPlannedEvent> Due;
	while (!PlannedEvents.IsEmpty() && CurrentSeconds >= PlannedEvents[0].Seconds)
	{
		Due.Add(PlannedEvents[0]);
		PlannedEvents.RemoveAt(0);
	}
	for (const FPlannedEvent& Event : Due)
	{
		FireEvent(Event, CurrentSeconds);
	}

	/*
	방치된 이상현상 악화
	*/
	TArray<AAnomalyBase*> ToEscalate;
	for (auto It = StartSeconds.CreateIterator(); It; ++It)
	{
		AAnomalyBase* Anomaly = It.Key().Get();
		if (!Anomaly)
		{
			It.RemoveCurrent();
			continue;
		}
		if (CurrentSeconds - It.Value() >= GameTime::MinutesToSeconds(Anomaly->EscalateAfterMinutes))
		{
			ToEscalate.Add(Anomaly);
		}
	}
	for (AAnomalyBase* Anomaly : ToEscalate)
	{
		Anomaly->Escalate();
	}
}

void UAnomalyManagerSubsystem::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase == EHotelPhase::Settlement)
	{
		FailRunningAnomalies();
		PlannedEvents.Reset();
	}
}

void UAnomalyManagerSubsystem::RecordResult(const AAnomalyBase* Anomaly)
{
	if (!Settlement || !Anomaly)
	{
		return;
	}
	FDailyAnomalyResult Result;
	Result.Id = Anomaly->AnomalyId;
	Result.DisplayName = Anomaly->DisplayName;
	Result.Status = Anomaly->GetState() == EAnomalyState::Resolved
		? EDailyAnomalyStatus::Resolved
		: EDailyAnomalyStatus::Unresolved;
	Settlement->RecordAnomaly(Result);
}

void UAnomalyManagerSubsystem::FailRunningAnomalies()
{
	for (AAnomalyBase* Anomaly : GetRunningAnomalies())
	{
		Anomaly->Fail();
	}
}

void UAnomalyManagerSubsystem::HandleDayStarted(int32 Day)
{
	PlannedEvents.Reset();
	PlanDay = Day;

	const UDataTable* Table = UHotelPhaseSettings::Get()->AnomalyScheduleTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogAnomaly, Warning, TEXT("[Schedule] AnomalyScheduleTable이 설정되지 않음 (Project Settings > Hotel Phase)"));
		return;
	}

	/*
	오늘 행만 골라 시각순 예약
	*/
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FAnomalyScheduleRow* Row = reinterpret_cast<const FAnomalyScheduleRow*>(Pair.Value);
		if (!Row || Row->Day != Day || Row->EventId.IsNone())
		{
			continue;
		}
		FPlannedEvent Event;
		Event.Kind = Row->Kind;
		Event.EventId = Row->EventId;
		Event.Seconds = GetScheduleSeconds(Day, Row->Hour, Row->Minute);
		PlannedEvents.Add(Event);
	}
	PlannedEvents.StableSort([](const FPlannedEvent& A, const FPlannedEvent& B)
	{
		return A.Seconds < B.Seconds;
	});

	UE_LOG(LogAnomaly, Log, TEXT("[Schedule] Day %d: %d개 예약"), Day, PlannedEvents.Num());
}

/*
18시 → 당일 18:00, 3시 → 다음 날 03:00
*/
double UAnomalyManagerSubsystem::GetScheduleSeconds(int32 Day, int32 Hour, int32 Minute) const
{
	const int32 DayStartHour = UHotelPhaseSettings::Get()->DayStartHour;
	const int32 AdjustedHour = Hour < DayStartHour ? Hour + 24 : Hour;
	return GameTime::GetDayStartSeconds(Day - 1)
		+ GameTime::HoursToSeconds(AdjustedHour)
		+ GameTime::MinutesToSeconds(Minute);
}

void UAnomalyManagerSubsystem::FireEvent(const FPlannedEvent& Event, double CurrentSeconds)
{
	switch (Event.Kind)
	{
	case EScheduleEventKind::Anomaly:
	{
		AAnomalyBase* Anomaly = FindAnomaly(Event.EventId);
		if (!Anomaly || !Anomaly->bAutoSchedule)
		{
			UE_LOG(LogAnomaly, Warning, TEXT("[Schedule] Day %d: '%s' 이상현상을 찾을 수 없음"), PlanDay, *Event.EventId.ToString());
			return;
		}
		if (!Anomaly->StartAnomaly())
		{
			/*
			시작 실패 (이미 진행 중 등): 1게임시간 뒤 다시 시도
			*/
			FPlannedEvent Retry = Event;
			Retry.Seconds = CurrentSeconds + GameTime::SecondsPerHour;
			const int32 Index = PlannedEvents.IndexOfByPredicate([&Retry](const FPlannedEvent& E)
			{
				return E.Seconds > Retry.Seconds;
			});
			PlannedEvents.Insert(Retry, Index == INDEX_NONE ? PlannedEvents.Num() : Index);
			UE_LOG(LogAnomaly, Log, TEXT("[Schedule] '%s' 시작 실패, 1시간 뒤 재시도"), *Event.EventId.ToString());
		}
		return;
	}
	case EScheduleEventKind::Omen:
		if (!PlayOmen(Event.EventId))
		{
			UE_LOG(LogAnomaly, Warning, TEXT("[Schedule] Day %d: '%s' 예고를 찾을 수 없음"), PlanDay, *Event.EventId.ToString());
		}
		return;
	case EScheduleEventKind::Scare:
		if (PlayScare(Event.EventId) == 0)
		{
			UE_LOG(LogAnomaly, Warning, TEXT("[Schedule] Day %d: '%s' 공포 연출을 찾을 수 없음"), PlanDay, *Event.EventId.ToString());
		}
		return;
	}
}

bool UAnomalyManagerSubsystem::PlayOmen(FName AnomalyId)
{
	for (TActorIterator<AOmenBase> It(GetWorld()); It; ++It)
	{
		if (It->GetRelatedAnomalyId() == AnomalyId)
		{
			It->PlayOmen();
			return true;
		}
	}
	return false;
}

/*
같은 ScareId를 가진 연출 전부 실행 (예: 3일차 소녀 201~204)
*/
int32 UAnomalyManagerSubsystem::PlayScare(FName ScareId)
{
	int32 Count = 0;
	for (TActorIterator<AScareBase> It(GetWorld()); It; ++It)
	{
		if (It->ScareId == ScareId)
		{
			It->Trigger();
			++Count;
		}
	}
	return Count;
}
