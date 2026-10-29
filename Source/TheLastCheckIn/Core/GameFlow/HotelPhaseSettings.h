#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HotelPhaseSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hotel Phase"))
class THELASTCHECKIN_API UHotelPhaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UHotelPhaseSettings();
	static const UHotelPhaseSettings* Get()
	{
		return GetDefault<UHotelPhaseSettings>();
	}
	UPROPERTY(Config, EditAnywhere, Category = "Time", meta = (ClampMin = "1.0"))
	double TimeRate = 60.0;
	
	UPROPERTY(Config, EditAnywhere, Category = "Days", meta = (ClampMin = "1"))
	int32 MaxDays = 10;
	
	UPROPERTY(Config, EditAnywhere, Category = "Time", meta = (ClampMin = "1.0"))
	int32 FirstAnomalyDay = 2;
	
	UPROPERTY(Config, EditAnywhere, Category = "Phase Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 DayStartHour = 6;
	UPROPERTY(Config, EditAnywhere, Category = "Phase Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 RestStartHour = 20;
	UPROPERTY(Config, EditAnywhere, Category = "Phase Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 NightStartHour = 22;
	UPROPERTY(Config, EditAnywhere, Category = "Phase Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 SettlementStartHour = 5;
	
	/*
	세부구간
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Day Slot Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 MiddayStartHour = 11;
	UPROPERTY(Config, EditAnywhere, Category = "Day Slot Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 CheckInStartHour = 14;
	UPROPERTY(Config, EditAnywhere, Category = "Day Slot Hours", meta = (ClampMin = "0", ClampMax = "23"))
	int32 EveningStartHour = 18;
};
