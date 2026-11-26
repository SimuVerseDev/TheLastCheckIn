#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScareBase.generated.h"

UCLASS(Abstract, Blueprintable)
class THELASTCHECKIN_API AScareBase : public AActor
{
	GENERATED_BODY()
public:
	AScareBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	FName ScareId;

	UFUNCTION(BlueprintImplementableEvent, Category = "Scare")
	void PlayScare();
	
	UFUNCTION(CallInEditor, Category = "Scare")
	void TestPlay()
	{
		PlayScare();
		Trigger();
	}
	
	/*
	시선대기 
	*/
	void Trigger();
	UFUNCTION(BlueprintImplementableEvent, Category = "Scare")
	void OnArmed();
	UFUNCTION(BlueprintImplementableEvent, Category = "Scare")
	void OnDisarmed();
	
protected:

	UPROPERTY(EditAnywhere, Category = "Scare|Look")
	bool bWaitForLook = false;
	
	UPROPERTY(EditAnywhere, Category = "Scare|Look", meta = (EditCondition = "bWaitForLook", MakeEditWidget))
	FVector LookTargetOffset = FVector(0, 0, 120);

	UPROPERTY(EditAnywhere, Category = "Scare|Look", meta = (EditCondition = "bWaitForLook", ClampMin = 0, ClampMax = 1))
	float LookDotThreshold = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Scare|Look", meta = (EditCondition = "bWaitForLook", Units = "cm"))
	float MaxLookDistance = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Scare|Look", meta = (EditCondition = "bWaitForLook", Units = "s"))
	float LookCheckInterval = 0.1f;
	
private:
	void CheckLook();
	void Disarm();
	
	FTimerHandle LookTimer;
};