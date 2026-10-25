#pragma once
#include "CoreMinimal.h"
#include "Housekeeping/Core/Dirt/HKDirtComponent.h"
#include "HKBedMakeEvaluator.generated.h"
class AHKBlanketActor;
class UBoxComponent;

/*
흐트러진 이불 : 침대 윗면을 덮은 비율 및 평평함으로 오염도 계산
*/
UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKBedMakeEvaluator : public UHKDirtComponent
{
	GENERATED_BODY()

public:
	UHKBedMakeEvaluator();

	virtual void ApplyDirt(FRandomStream& Random) override;

	/*
	평가요소
	*/
	UFUNCTION(BlueprintPure, Category="Housekeeping|Dirt|Bed")
	float GetCoverRatio() const
	{
		return LastCoverRatio;
	}

	UFUNCTION(BlueprintPure, Category="Housekeeping|Dirt|Bed")
	float GetFlatness() const
	{
		return LastFlatness;
	}
	
protected:
	virtual void BeginPlay() override;
	virtual float EvaluateDirt() const override;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed")
	TObjectPtr<AHKBlanketActor> Blanket;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed")
	FName BedTopTag = TEXT("BedTop");
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed", meta=(ClampMin=0, Units="cm"))
	float AutoFindRadius = 400.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed", meta=(ClampMin=1, ClampMax=32))
	int32 CoverGridX = 6;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed", meta=(ClampMin=1, ClampMax=32))
	int32 CoverGridY = 8;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed", meta=(ClampMin=0, ClampMax=1))
	float CoverWeight = 0.7f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Bed", meta=(ClampMin=1, Units="cm"))
	float MaxHeightDeviation = 10.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Scatter", meta=(ClampMin=0, Units="cm"))
	float ScatterOffset = 50.0f;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> BedArea;
	
	// DEBUG
	mutable float LastCoverRatio = 0.0f;
	mutable float LastFlatness = 0.0f;
};