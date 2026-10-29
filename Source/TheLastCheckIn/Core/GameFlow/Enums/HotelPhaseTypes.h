#pragma once
#include "CoreMinimal.h"
#include "HotelPhaseTypes.generated.h"

UENUM(BlueprintType)
enum class EHotelPhase : uint8
{
	Day        UMETA(DisplayName = "Day"),         // 06:00 ~ 20:00
	Rest       UMETA(DisplayName = "Rest"),        // 20:00 ~ 22:00
	Night      UMETA(DisplayName = "Night"),       // 22:00 ~ 05:00
	Settlement UMETA(DisplayName = "Settlement")   // 05:00 ~ 06:00
};

UENUM(BlueprintType)
enum class EDaySlot : uint8
{
	None UMETA(DisplayName = "None"),
	Checkout UMETA(DisplayName = "Checkout"),   // 06:00 ~ 11:00
	Midday   UMETA(DisplayName = "Midday"),     // 11:00 ~ 14:00 (2일차~ 이상현상)
	CheckIn  UMETA(DisplayName = "CheckIn"),    // 14:00 ~ 18:00
	Evening  UMETA(DisplayName = "Evening")     // 18:00 ~ 20:00 (2일차~ 이상현상)
};