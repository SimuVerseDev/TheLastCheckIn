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
	PlannedStartSeconds.Reset();
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

AAnomalyBase* UAnomalyManagerSubsystem::StartRandomAnomaly()
{
	TArray<AAnomalyBase*> Candidates;
	for (const TWeakObjectPtr<AAnomalyBase>& Weak : Anomalies)
	{
		AAnomalyBase* Anomaly = Weak.Get();
		if (Anomaly && !Anomaly->IsRunning())
		{
			Candidates.Add(Anomaly);
		}
	}
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	AAnomalyBase* Picked = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	return Picked->StartAnomaly() ? Picked : nullptr;
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

/*
HandleTimeAdvanced -> Fail -> HandleStartFailed
*/
void UAnomalyManagerSubsystem::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	/*
	예약된 예고 재생
	*/
	if (AOmenBase* Omen = PlannedOmen.Get(); Omen && CurrentSeconds >= PlannedOmenSeconds)
	{
		PlannedOmen.Reset();
		Omen->PlayOmen();
	}

	/*
	예약 시각이 지난 이상현상 시작
	*/
	TArray<AAnomalyBase*> ToStart;
	for (auto It = PlannedStartSeconds.CreateIterator(); It; ++It)
	{
		AAnomalyBase* Anomaly = It.Key().Get();
		if (!Anomaly)
		{
			It.RemoveCurrent();
			continue;
		}
		if (CurrentSeconds >= It.Value())
		{
			ToStart.Add(Anomaly);
			It.RemoveCurrent();
		}
	}
	for (AAnomalyBase* Anomaly : ToStart)
	{
		/*
		bool bStarted = Anomaly->StartAnomaly();
		if (!bStarted)
		{
			HandleStartFailed(Anomaly, CurrentSeconds);
		}
		*/
		if (!Anomaly->StartAnomaly())
		{
			HandleStartFailed(Anomaly, CurrentSeconds);
		}
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
		PlannedStartSeconds.Reset();
		PlannedOmen.Reset();
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
	PlannedStartSeconds.Reset();
	PlannedOmen.Reset();
	PlanDay = Day;
	FixedTonight.Reset();
	SpareIds.Reset();

	TSet<FName> Learned;
	const FAnomalyNightPlan* Plan = FindPlanRow(Day, Learned);

	if (!Plan)
	{
		LastNightIds.Reset();
		return;
	}

	/*
	고정 (소개 / 조합)
	*/
	TArray<FName> Tonight;
	for (const FName& Id : Plan->Fixed)
	{
		Tonight.AddUnique(Id);
	}

	/*
	복습 후보 = 소개된 것 - 오늘 고정, 어제 안 나온 것을 앞에
	*/
	TArray<FName> Fresh;
	TArray<FName> Repeat;
	for (const FName& Id : Learned)
	{
		if (!Tonight.Contains(Id))
		{
			(LastNightIds.Contains(Id) ? Repeat : Fresh).Add(Id);
		}
	}
	auto Shuffle = [](TArray<FName>& Arr)
	{
		for (int32 i = Arr.Num() - 1; i > 0; --i)
		{
			Arr.Swap(i, FMath::RandRange(0, i));
		}
	};
	Shuffle(Fresh);
	Shuffle(Repeat);
	Fresh.Append(Repeat);

	const int32 ExtraMax = FMath::Max(Plan->ExtraMin, Plan->ExtraMax);
	const int32 Extra = FMath::Min(FMath::RandRange(Plan->ExtraMin, ExtraMax), Fresh.Num());
	for (int32 i = 0; i < Extra; ++i)
	{
		Tonight.Add(Fresh[i]);
	}
	for (int32 i = Extra; i < Fresh.Num(); ++i) // 안 뽑힌 나머지 = 대체 후보
	{
		SpareIds.Add(Fresh[i]);
	}

	/*
	예약
	*/
	LastNightIds.Reset();
	for (const FName& Id : Tonight)
	{
		AAnomalyBase* Anomaly = FindAnomaly(Id);
		if (!Anomaly || !Anomaly->bAutoSchedule)
		{
			UE_LOG(LogAnomaly, Warning, TEXT("[NightPlan] Day %d: '%s' 이상현상을 찾을 수 없음"), Day, *Id.ToString());
			continue;
		}
		PlanStart(Anomaly, Day);
		if (Plan->Fixed.Contains(Id))
		{
			FixedTonight.Add(Anomaly);
		}
		LastNightIds.Add(Id);
	}
	UE_LOG(LogAnomaly, Log, TEXT("[NightPlan] Day %d: %s"), Day, *FString::JoinBy(Tonight, TEXT(", "), [](const FName& N) { return N.ToString(); }));

	PlanOmen();
}

/*
22시 → 당일 22:00, 3시 → 다음 날 03:00
*/
double UAnomalyManagerSubsystem::GetNightSeconds(int32 Day, int32 Hour) const
{
	const int32 DayStartHour = UHotelPhaseSettings::Get()->DayStartHour;
	const int32 AdjustedHour = Hour < DayStartHour ? Hour + 24 : Hour;
	return GameTime::GetDayStartSeconds(Day - 1) + GameTime::HoursToSeconds(AdjustedHour);
}

const FAnomalyNightPlan* UAnomalyManagerSubsystem::FindPlanRow(int32 Day, TSet<FName>& OutLearned) const
{
	OutLearned.Reset();
	/*
	DataTable 동기로드
	*/
	const UDataTable* Table = UHotelPhaseSettings::Get()->AnomalyNightPlanTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogAnomaly, Warning, TEXT("[NightPlan] AnomalyNightPlanTable이 설정되지 않음 (Project Settings > Hotel Phase)"));
		return nullptr;
	}
	const FAnomalyNightPlan* Best = nullptr;
	int32 BestDay = 0;
	
	/*
	DT 순회
	*/
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		int32 RowDay = 0;
		if (!LexTryParseString(RowDay, *Pair.Key.ToString()) || RowDay > Day)
		{
			continue;
		}
		
		const FAnomalyNightPlan* Row = reinterpret_cast<const FAnomalyNightPlan*>(Pair.Value);
		
		OutLearned.Append(Row->Fixed);
		OutLearned.Append(Row->Pool);
		if (RowDay > BestDay)
		{
			BestDay = RowDay;
			Best = Row;
		}
	}
	return Best;
}

/*
아침에 오늘 밤 예정된 이상현상의 예고를 하나 골라 낮(06~20시) 중에 예약
*/
void UAnomalyManagerSubsystem::PlanOmen()
{
	PlannedOmen.Reset();

	const UHotelPhaseSettings* Settings = UHotelPhaseSettings::Get();
	const double DayStart = GetNightSeconds(PlanDay, Settings->DayStartHour);
	const double RestStart = GetNightSeconds(PlanDay, Settings->RestStartHour);

	TArray<AOmenBase*> Candidates;
	for (TActorIterator<AOmenBase> It(GetWorld()); It; ++It)
	{
		AAnomalyBase* Anomaly = FindAnomaly(It->GetRelatedAnomalyId());
		if (Anomaly && PlannedStartSeconds.Contains(Anomaly)) // 오늘 밤 예약된 것만
		{
			Candidates.Add(*It);
		}
	}
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogAnomaly, Log, TEXT("[Omen] Day %d: 예고 후보 없음"), PlanDay);
		return;
	}

	AOmenBase* Omen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	AAnomalyBase* Anomaly = FindAnomaly(Omen->GetRelatedAnomalyId());
	const double Latest = FMath::Min(RestStart, PlannedStartSeconds[Anomaly]); // 낮 안에, 본편보다 먼저

	PlannedOmen = Omen;
	PlannedOmenSeconds = FMath::FRandRange(DayStart, FMath::Max(DayStart, Latest));
	UE_LOG(LogAnomaly, Log, TEXT("[Omen] Day %d: %s 예고 예약"), PlanDay, *Omen->GetRelatedAnomalyId().ToString());
}

bool UAnomalyManagerSubsystem::PlanStart(AAnomalyBase* Anomaly, int32 Day, double NotBefore)
{
	const double Earliest = FMath::Max(GetNightSeconds(Day, Anomaly->EarliestStartHour), NotBefore);
	const double Latest = GetNightSeconds(Day, Anomaly->LatestStartHour);
	if (Latest < Earliest)
	{
		return false;
	}
	PlannedStartSeconds.Add(Anomaly, FMath::FRandRange(Earliest, Latest));
	return true;
}

void UAnomalyManagerSubsystem::HandleStartFailed(AAnomalyBase* Anomaly, double CurrentSeconds)
{
	constexpr double RetryDelay = GameTime::SecondsPerHour; // 1 게임시간

	/*
	고정: 1시간 뒤 다시 시도
	*/
	if (FixedTonight.Contains(Anomaly))
	{
		if (!PlanStart(Anomaly, PlanDay, CurrentSeconds + RetryDelay))
		{
			UE_LOG(LogAnomaly, Warning, TEXT("[NightPlan] '%s' 오늘 밤 시작 실패 (시간 초과)"), *Anomaly->AnomalyId.ToString());
		}
		return;
	}

	/*
	복습: 남은 후보로 대체 (어제 안 나온 것부터)
	*/
	LastNightIds.Remove(Anomaly->AnomalyId);
	while (!SpareIds.IsEmpty())
	{
		const FName Id = SpareIds[0];
		SpareIds.RemoveAt(0);

		AAnomalyBase* Spare = FindAnomaly(Id);
		if (Spare && Spare->bAutoSchedule && !Spare->IsRunning() && PlanStart(Spare, PlanDay, CurrentSeconds))
		{
			LastNightIds.Add(Id);
			return;
		}
	}
}
