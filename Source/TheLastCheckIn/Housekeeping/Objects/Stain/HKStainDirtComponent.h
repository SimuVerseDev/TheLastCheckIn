#pragma once
#include "CoreMinimal.h"
#include "Housekeeping/Core/Dirt/HKDirtComponent.h"
#include "HKStainDirtComponent.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnStainAmountChanged, float, Amount);

UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKStainDirtComponent : public UHKDirtComponent
{
	GENERATED_BODY()
public:
	virtual void ApplyDirt(FRandomStream& Random) override;
	virtual void ClearDirt() override;
	
	/*
	걸레가 움직인 거리값 누적으로 Amount감소
	*/
	UFUNCTION(BlueprintCallable, Category = "Housekeeping|Dirt|Stain")
	void AddWipe(float Distance);
	
	UFUNCTION(BlueprintPure, Category = "Housekeeping|Dirt|Stain")
	float GetAmount() const
	{
		return Amount;
	}
	
	UPROPERTY(BlueprintAssignable, Category = "Housekeeping|Dirt|Stain")
	FHKOnStainAmountChanged OnAmountChanged;
	
protected:
	virtual float EvaluateDirt() const override // Dirt평가
	{
		return Amount;
	}
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Dirt|Stain", meta=(ClampMin=1, Units="cm"))
	float WipeDistanceToClean = 600.0f;
	
private:
	void SetAmount(float NewAmount);
	float Amount = 0.0f;
};
