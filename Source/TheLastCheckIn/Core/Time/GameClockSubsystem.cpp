#include "Core/Time/GameClockSubsystem.h"
#include "Engine/World.h"

void UGameClockSubsystem::StartClock(double InitialGameSeconds, double InTimeRate)
{
	UWorld* World = GetWorld();

	if (!World || !World->IsGameWorld())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(ClockTimerHandle);
	bRunning = false;

	TotalGameSeconds = FMath::Max(0.0, InitialGameSeconds);
	TimeRate = FMath::Max(0.001, InTimeRate);

	ResumeClock();
}

void UGameClockSubsystem::StopClock()
{
	if (!bRunning)
	{
		return;
	}

	// 마지막 타이머 호출 이후의 시간까지 반영
	UpdateClock();

	bRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimerHandle);
	}
}

void UGameClockSubsystem::ResumeClock()
{
	UWorld* World = GetWorld();

	if (!World || !World->IsGameWorld() || bRunning)
	{
		return;
	}

	// 정지해 있던 시간은 게임 시간에 더하지 않음
	LastWorldSeconds = World->GetTimeSeconds();
	bRunning = true;

	World->GetTimerManager().SetTimer(
		ClockTimerHandle,
		this,
		&UGameClockSubsystem::UpdateClock,
		UpdateInterval,
		true);
}

int32 UGameClockSubsystem::GetDay() const
{
	return static_cast<int32>(
		FMath::FloorToInt64(TotalGameSeconds / 86400.0)) + 1;
}

int32 UGameClockSubsystem::GetHour() const
{
	return static_cast<int32>(
		FMath::FloorToInt64(TotalGameSeconds / 3600.0) % 24);
}

int32 UGameClockSubsystem::GetMinute() const
{
	return static_cast<int32>(
	FMath::FloorToInt64(TotalGameSeconds / 60.0) % 60);
}

void UGameClockSubsystem::Deinitialize()
{
	bRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimerHandle);
	}

	OnTimeAdvanced.Clear();

	Super::Deinitialize();
}

void UGameClockSubsystem::UpdateClock()
{
	UWorld* World = GetWorld();

	if (!World || !bRunning)
	{
		return;
	}

	const double CurrentWorldSeconds = World->GetTimeSeconds();
	const double ElapsedSeconds =
		FMath::Max(0.0, CurrentWorldSeconds - LastWorldSeconds);

	LastWorldSeconds = CurrentWorldSeconds;

	if (ElapsedSeconds <= 0.0)
	{
		return;
	}

	const double PreviousSeconds = TotalGameSeconds;

	TotalGameSeconds += ElapsedSeconds * TimeRate;

	OnTimeAdvanced.Broadcast(PreviousSeconds, TotalGameSeconds);
}
