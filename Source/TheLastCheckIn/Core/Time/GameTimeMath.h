#pragma once

#include "CoreMinimal.h"

namespace GameTime
{
	inline constexpr double SecondsPerMinute = 60.0;
	inline constexpr double SecondsPerHour = 3600.0;
	inline constexpr double SecondsPerDay = 86400.0;

	inline constexpr double MinutesToSeconds(double Minutes)
	{
		return Minutes * SecondsPerMinute;
	}

	inline constexpr double HoursToSeconds(double Hours)
	{
		return Hours * SecondsPerHour;
	}

	inline int64 GetDayIndex(double TotalSeconds)
	{
		return FMath::FloorToInt64(TotalSeconds / SecondsPerDay);
	}

	inline double GetDayStartSeconds(int64 DayIndex)
	{
		return static_cast<double>(DayIndex) * SecondsPerDay;
	}

	inline double GetSecondsOfDay(double TotalSeconds)
	{
		return TotalSeconds - GetDayStartSeconds(GetDayIndex(TotalSeconds));
	}
}
