#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/GameFlow/HotelPhaseSettings.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHotelPhase, Log, All);

namespace
{
	constexpr double SecondsPerHour = 3600.0;
	constexpr double SecondsPerDay = 86400.0;
}

void UHotelPhaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	Clock = Collection.InitializeDependency<UGameClockSubsystem>();
}

void UHotelPhaseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	if (InWorld.IsGameWorld())
	{
		StartRun();
	}
}

void UHotelPhaseSubsystem::Deinitialize()
{
	if (Clock)
	{
		Clock->OnTimeAdvanced.RemoveAll(this);
	}

	OnPhaseChanged.Clear();
	OnDaySlotChanged.Clear();
	OnDayStarted.Clear();
	OnRunCompleted.Clear();

	Super::Deinitialize();
}


void UHotelPhaseSubsystem::StartRun()
{
	if (!Clock)
	{
		return;
	}

	// 순서가 어긋나면 일부 페이즈(특히 Settlement)에 진입하지 못함
	if (!UHotelPhaseSettings::Get()->HasValidHourOrder())
	{
		UE_LOG(LogHotelPhase, Error,
			TEXT("Invalid phase hours. Check Project Settings > Game > Hotel Phase."));
		return;
	}

	if (!Clock->OnTimeAdvanced.IsAlreadyBound(this, &UHotelPhaseSubsystem::HandleTimeAdvanced))
	{
		Clock->OnTimeAdvanced.AddDynamic(this, &UHotelPhaseSubsystem::HandleTimeAdvanced);
	}

	bRunActive = true;
	CurrentDay = 1;

	StartDay();
}


void UHotelPhaseSubsystem::ConfirmSettlement()
{
	if (!bRunActive || CurrentPhase != EHotelPhase::Settlement)
	{
		return;
	}

	if (CurrentDay >= UHotelPhaseSettings::Get()->MaxDays)
	{
		bRunActive = false;

		UE_LOG(LogHotelPhase, Log, TEXT("Run completed on day %d"), CurrentDay);
		OnRunCompleted.Broadcast(CurrentDay);
		return;
	}

	++CurrentDay;
	StartDay();
}

bool UHotelPhaseSubsystem::IsDayAnomalyAllowed() const
{
	return CurrentPhase == EHotelPhase::Day
		&& CurrentDay >= UHotelPhaseSettings::Get()->FirstAnomalyDay
		&& (CurrentDaySlot == EDaySlot::Midday || CurrentDaySlot == EDaySlot::Evening);
}

void UHotelPhaseSubsystem::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	if (!bRunActive || CurrentPhase == EHotelPhase::Settlement)
	{
		return;
	}

	const UHotelPhaseSettings* Settings = UHotelPhaseSettings::Get();
	const int32 Hour = Clock->GetHour();

	EHotelPhase TargetPhase = EvaluatePhase(Hour);

	if (TargetPhase == EHotelPhase::Night
		&& CurrentDay == 1
		&& Settings->bSkipNightOnFirstDay)
	{
		TargetPhase = EHotelPhase::Settlement;
	}

	if (TargetPhase == EHotelPhase::Settlement)
	{
		EnterSettlement();
		return;
	}

	SetPhase(TargetPhase);
	SetDaySlot(TargetPhase == EHotelPhase::Day ? EvaluateDaySlot(Hour) : EDaySlot::None);
}

void UHotelPhaseSubsystem::StartDay()
{
	const UHotelPhaseSettings* Settings = UHotelPhaseSettings::Get();

	const double DayStartSeconds =
		(CurrentDay - 1) * SecondsPerDay
		+ Settings->DayStartHour * SecondsPerHour;

	Clock->StartClock(DayStartSeconds, Settings->TimeRate);

	SetPhase(EHotelPhase::Day);
	SetDaySlot(EvaluateDaySlot(Settings->DayStartHour));

	UE_LOG(LogHotelPhase, Log, TEXT("Day %d started"), CurrentDay);
	OnDayStarted.Broadcast(CurrentDay);
}

void UHotelPhaseSubsystem::EnterSettlement()
{
	SetDaySlot(EDaySlot::None);

	SetPhase(EHotelPhase::Settlement);

	Clock->StopClock();
}

void UHotelPhaseSubsystem::SetPhase(EHotelPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	const EHotelPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	UE_LOG(LogHotelPhase, Log, TEXT("Day %d: %s -> %s"),
		CurrentDay,
		*UEnum::GetValueAsString(OldPhase),
		*UEnum::GetValueAsString(NewPhase));

	OnPhaseChanged.Broadcast(OldPhase, NewPhase);
}

void UHotelPhaseSubsystem::SetDaySlot(EDaySlot NewSlot)
{
	if (CurrentDaySlot == NewSlot)
	{
		return;
	}

	const EDaySlot OldSlot = CurrentDaySlot;
	CurrentDaySlot = NewSlot;

	OnDaySlotChanged.Broadcast(OldSlot, NewSlot);
}

EHotelPhase UHotelPhaseSubsystem::EvaluatePhase(int32 Hour) const
{
	const UHotelPhaseSettings* Settings = UHotelPhaseSettings::Get();

	if (Hour >= Settings->DayStartHour && Hour < Settings->RestStartHour)
	{
		return EHotelPhase::Day;
	}

	if (Hour >= Settings->RestStartHour && Hour < Settings->NightStartHour)
	{
		return EHotelPhase::Rest;
	}

	if (Hour >= Settings->SettlementStartHour && Hour < Settings->DayStartHour)
	{
		return EHotelPhase::Settlement;
	}

	return EHotelPhase::Night;
}

EDaySlot UHotelPhaseSubsystem::EvaluateDaySlot(int32 Hour) const
{
	const UHotelPhaseSettings* Settings = UHotelPhaseSettings::Get();

	if (Hour < Settings->MiddayStartHour)
	{
		return EDaySlot::Checkout;
	}

	if (Hour < Settings->CheckInStartHour)
	{
		return EDaySlot::Midday;
	}

	if (Hour < Settings->EveningStartHour)
	{
		return EDaySlot::CheckIn;
	}

	return EDaySlot::Evening;
}