#include "Core/Economy/HotelEconomySubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/Economy/HotelEconomySettings.h"
DEFINE_LOG_CATEGORY_STATIC(LogHotelEconomy, Log, All);

void UHotelEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	if (Phase)
	{
		Phase->OnDayStarted.AddDynamic(this, &UHotelEconomySubsystem::HandleDayStarted);
	}
}

void UHotelEconomySubsystem::Deinitialize()
{
	if (Phase)
	{
		Phase->OnDayStarted.RemoveDynamic(this, &UHotelEconomySubsystem::HandleDayStarted);
	}
	Super::Deinitialize();
}

void UHotelEconomySubsystem::AddIncome(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return;
	}
	Capital += Amount;
	TodayIncome += Amount;
	UE_LOG(LogHotelEconomy, Log, TEXT("Income +%.0f (today %.0f, capital %.0f)"),
		Amount, TodayIncome, Capital);
}

void UHotelEconomySubsystem::HandleDayStarted(int32 Day)
{
	if (Day == 1)
	{
		const float Initial = GetDefault<UHotelEconomySettings>()->StartingCapital;
		Capital = FMath::IsFinite(Initial) ? FMath::Max(0.0f, Initial) : 0.0f;
	}
	TodayIncome = 0.0f;
}

bool UHotelEconomySubsystem::TrySpend(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || Capital < Amount) return false;
	Capital -= Amount;
	return true;
}


