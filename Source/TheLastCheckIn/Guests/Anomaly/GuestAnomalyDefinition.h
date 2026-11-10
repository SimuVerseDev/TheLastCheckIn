#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GuestAnomalyDefinition.generated.h"

class UAnimMontage;
class UGuestAnomalyBehavior;

UCLASS(BlueprintType)
class THELASTCHECKIN_API UGuestAnomalyDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly")
	FText DisplayName;

	// 실제로 실행할 C++ 행동 클래스.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly")
	TSubclassOf<UGuestAnomalyBehavior> BehaviorClass;

	// 이 신호를 받으면 해결 횟수를 누적
	// 예: Crowbar, Talk, SpecialItem
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Resolution")
	FName ResolutionSignal = TEXT("Crowbar");

	// 해결에 필요한 신호 횟수
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Anomaly|Resolution",
		meta = (ClampMin = "1"))
	int32 RequiredSignals = 1;

	// 행동 중 재생할 애니메이션
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Animation")
	TObjectPtr<UAnimMontage> ActionMontage;

	// 해결됐을 때 재생할 애니메이션
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anomaly|Animation")
	TObjectPtr<UAnimMontage> ResolvedMontage;

	// 행동을 반복하는 최소 간격.
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Anomaly|Timing",
		meta = (ClampMin = "0.1"))
	float ActionIntervalSeconds = 3.0f;
};