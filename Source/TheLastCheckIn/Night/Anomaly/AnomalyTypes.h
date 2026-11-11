#pragma once

#include "CoreMinimal.h"
#include "AnomalyTypes.generated.h"

UENUM(BlueprintType)
enum class EAnomalyState : uint8
{
	Idle,
	Active,
	Escalated,  // 방치되어 악화
	Resolved,
	Failed
};