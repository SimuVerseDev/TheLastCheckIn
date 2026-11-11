#include "Night/Anomaly/AnomalyManagerSubsystem.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "Night/Settlement/DailySettlementSubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "EngineUtils.h"

void UAnomalyManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Clock = Collection.InitializeDependency<UGameClockSubsystem>();
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	Settlement = Collection.InitializeDependency<UDailySettlementSubsystem>();
	Clock->OnTimeAdvanced.AddDynamic(this, &ThisClass::HandleTimeAdvanced);
	Phase->OnPhaseChanged.AddDynamic(this, &ThisClass::HandlePhaseChanged);
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
}

void UAnomalyManagerSubsystem::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	TArray<AAnomalyBase*> ToEscalate;
	for (auto It = StartSeconds.CreateIterator(); It; ++It)
	{
		AAnomalyBase* Anomaly = It.Key().Get();
		if (!Anomaly)
		{
			It.RemoveCurrent();
			continue;
		}
		if (CurrentSeconds - It.Value() >= Anomaly->EscalateAfterMinutes * 60.0)
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
