// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Night/Anomaly/Light/LightAnomaly.h"
#include "AnomalyLightComponent.generated.h"

class ULightComponent;

// 발광 재질도 함께 변경하고 싶을 때 BP에서 연결할 알림입니다.
// 0 = 소등, 1 = 원래 발광 상태
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAnomalyLightOutputChanged,
	float, Multiplier);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THELASTCHECKIN_API UAnomalyLightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAnomalyLightComponent();

	// 스위치 및 LightAnomaly의 구역 ID와 일치해야 합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	FName ZoneId = NAME_None;

	// 비워 두면 레벨에서 LightAnomaly 하나를 찾습니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	TObjectPtr<ALightAnomaly> Anomaly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light",
		meta = (ClampMin = "0.02", Units = "s"))
	float MinFlickerInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light",
		meta = (ClampMin = "0.02", Units = "s"))
	float MaxFlickerInterval = 0.35f;

	UPROPERTY(BlueprintAssignable, Category = "Night|Light")
	FAnomalyLightOutputChanged OnLightOutputChanged;
	
	// 구역 액터가 조명을 자동 연결할 때 사용합니다.
	bool AssignToZone(
		AActor* ZoneOwner,
		ALightAnomaly* InAnomaly,
		FName InZoneId);

	// 구역 액터가 종료될 때 연결 해제
	void ReleaseFromZone(AActor* ZoneOwner);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleLightingChanged(EHotelLightMode Mode, FName AffectedZoneId);

	void CaptureOriginalState();
	void RestoreOriginalState();
	void ApplyOutput(bool bOn);
	void ScheduleNextFlicker();
	void ToggleFlicker();

	// 같은 액터에 들어 있는 조명들을 보관합니다.
	TArray<TWeakObjectPtr<ULightComponent>> Lights;
	TArray<bool> OriginalVisibility;

	FTimerHandle FlickerTimer;

	bool bHasSnapshot = false;
	bool bFlickering = false;
	bool bOutputOn = true;
	
	// 한 조명이 여러 구역에 중복 연결되는 것을 방지합니다.
	TWeakObjectPtr<AActor> AssignedZoneOwner;
};