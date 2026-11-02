#include "Core/Economy/HotelEconomySubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
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
	if (Amount <= 0.0f)
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
	TodayIncome = 0.0f;
}


