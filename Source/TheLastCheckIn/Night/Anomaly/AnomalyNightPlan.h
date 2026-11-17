#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AnomalyNightPlan.generated.h"

USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FAnomalyNightPlan : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Plan")
	TArray<FName> Fixed;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Plan", meta = (ClampMin="0"))
	int32 ExtraMin = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Plan", meta = (ClampMin="0"))
	int32 ExtraMax = 0;
};