#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HKDirtComponent.generated.h"

UCLASS(Abstract, ClassGroup=(Housekeeping))
class UHKDirtComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHKDirtComponent();
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Dirt")
	float GetRawDirt() const;
	
	// Tolerance보다 낮으면 0으로 최종반환
	UFUNCTION(BlueprintPure, Category="Housekeeping|Dirt")
	float GetDirt() const;
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Dirt")
	bool IsDirtActive() const
	{
		return bDirtActive;
	}

	UFUNCTION(BlueprintPure, Category="Housekeeping|Dirt")
	bool IsCleaned() const
	{
		return GetDirt() <= 0.0f;
	}

	float GetWeight() const
	{
		return Weight;
	}

	bool IsRandomCandidate() const
	{
		return bRandomCandidate;
	}

	const FTransform& GetHomeTransform() const
	{
		return HomeTransform;
	}

	virtual void ApplyDirt(FRandomStream& Random);
	virtual void ClearDirt();

protected:
	
	virtual void BeginPlay() override;
	virtual float EvaluateDirt() const
	{
		return 0.0f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Housekeeping|Dirt", meta=(ClampMin=0))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Housekeeping|Dirt", meta=(ClampMin=0, ClampMax=1))
	float Tolerance = 0.1f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Housekeeping|Dirt")
	bool bRandomCandidate = true;

private:
	bool bDirtActive = false;
	FTransform HomeTransform;
};