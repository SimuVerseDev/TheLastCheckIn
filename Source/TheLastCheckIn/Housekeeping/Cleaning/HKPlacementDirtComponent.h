#pragma once
#include "CoreMinimal.h"
#include "Housekeeping/Cleaning/HKDirtComponent.h"
#include "HKPlacementDirtComponent.generated.h"

/*
흐트러진 가구 - 원래 위치 대비 거리·회전 오차로 오염도 계산
*/
UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKPlacementDirtComponent : public UHKDirtComponent
{
	GENERATED_BODY()

public:
	virtual void ApplyDirt(FRandomStream& Random) override;

protected:
	virtual void BeginPlay() override;
	virtual float EvaluateDirt() const override;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Placement", meta=(ClampMin=1, Units="cm"))
	float MaxDistance = 100.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Placement", meta=(ClampMin=1, ClampMax=180, Units="deg"))
	float MaxAngle = 90.0f;

	/*
	흐트러뜨리기 범위
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Scatter", meta=(ClampMin=0, Units="cm"))
	float ScatterMinOffset = 40.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Scatter", meta=(ClampMin=0, Units="cm"))
	float ScatterMaxOffset = 120.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Scatter", meta=(ClampMin=0, ClampMax=180, Units="deg"))
	float ScatterMaxYaw = 60.0f;

	/*
	옆으로 쓰러질 확률
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Scatter", meta=(ClampMin=0, ClampMax=1))
	float TipOverChance = 0.2f;
};
