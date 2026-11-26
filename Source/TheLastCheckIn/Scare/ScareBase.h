#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScareBase.generated.h"

class UBoxComponent;
class ULightComponent;

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
	
	/*
	구역 조명 (LightArea 박스 안 라이트 켜기/끄기)
	*/
	UFUNCTION(BlueprintCallable, Category = "Scare|Light")
	void SetAreaLightsOn(bool bOn);
	
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
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scare|Light")
	TObjectPtr<UBoxComponent> LightArea;
	
private:
	void CheckLook();
	void Disarm();
	void CollectAreaLights();
	
	FTimerHandle LookTimer;
	
	TArray<TWeakObjectPtr<ULightComponent>> AreaLights;
	TArray<bool> AreaLightVisible;
	bool bAreaLightsCollected = false;
};