#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "HotelPhaseSubsystem.generated.h"

class UGameClockSubSystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHotelPhaseChanged,
	EHotelPhase, OldPhase,
	EHotelPhase, NewPhase);
