#pragma once

#include "CoreMinimal.h"
#include "DailySettlementTypes.generated.h"

UENUM(BlueprintType)
enum class EDayEndReason : uint8
{
	Automatic,
	Sleep
};

UENUM(BlueprintType)
enum class EDailyAnomalyStatus : uint8
{
	NotOccurred,
	Unresolved,
	Resolved
};

USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FDailyAnomalyResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EDailyAnomalyStatus Status = EDailyAnomalyStatus::NotOccurred;
};

// A copy of the completed operating day, independent of the clock's calendar day.
USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FDailySettlementData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Day = 0;
	UPROPERTY(BlueprintReadOnly) EDayEndReason EndReason = EDayEndReason::Automatic;
	UPROPERTY(BlueprintReadOnly) float Income = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Capital = 0.0f;
	// False means the reputation system has not supplied a result yet.
	UPROPERTY(BlueprintReadOnly) bool bHasReputation = false;
	UPROPERTY(BlueprintReadOnly) int32 ReputationBefore = 0;
	UPROPERTY(BlueprintReadOnly) int32 ReputationAfter = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FDailyAnomalyResult> Anomalies;
};
