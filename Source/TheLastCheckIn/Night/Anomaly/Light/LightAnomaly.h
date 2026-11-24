// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "LightAnomaly.generated.h"

class UGameClockSubsystem;
class UAnomalyLightComponent;

// 조명에 적용할 상태
UENUM(BlueprintType)
enum class EHotelLightMode : uint8
{
	Normal,      // 정상
	Flickering,  // 특정 구역 깜빡임
	Blackout     // 호텔 전체 정전
};

// 조명 컴포넌트가 나중에 이 알림을 받아 실제 조명을 제어합니다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHotelLightingChanged,
	EHotelLightMode, Mode,
	FName, ZoneId);

UCLASS()
class THELASTCHECKIN_API ALightAnomaly : public AAnomalyBase
{
	GENERATED_BODY()

public:
	ALightAnomaly();

	virtual bool StartAnomaly() override;
	virtual bool Escalate() override;
	virtual bool Resolve() override;
	virtual bool Fail() override;

	// 해당 구역의 스위치에서 호출
	UFUNCTION(BlueprintCallable, Category = "Night|Light")
	bool TryResolveFromSwitch(FName SwitchZoneId);

	// 직원실 차단기에서 호출
	UFUNCTION(BlueprintCallable, Category = "Night|Light")
	bool TryRestoreFromBreaker();
	
	// 이번 이상현상의 전화를 받았는지 확인
	UFUNCTION(BlueprintPure, Category = "Night|Light")
	bool WasComplaintAnswered() const
	{
		return bComplaintAnswered;
	}

	// 이번 이상현상의 통화가 끝났는지 확인
	UFUNCTION(BlueprintPure, Category = "Night|Light")
	bool WasComplaintFinished() const
	{
		return bComplaintFinished;
	}

	UPROPERTY(BlueprintAssignable, Category = "Night|Light")
	FHotelLightingChanged OnLightingChanged;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	EHotelLightMode LightingMode = EHotelLightMode::Normal;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	FName ActiveZoneId = NAME_None;

	// Standalone RectLight actors also follow whole-hotel blackouts, even outside light zones.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	bool bAutoConnectRectLights = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 실제 조명과 스위치에 지정할 구역 ID와 동일하게 설정합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	TArray<FName> FlickerZoneIds;

	// 이상현상이 시작될 때 곧바로 정전될 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ImmediateBlackoutChance = 0.2f;

	// 실제 시간이 아닌 게임 시간 기준
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light",
		meta = (ClampMin = "1"))
	int32 ComplaintDelayMinutes = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	FText BlackoutComplaint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	FText RestoredComplaint;

private:
	UFUNCTION()
	void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

	void SetLightingMode(EHotelLightMode NewMode);
	void ConnectRectLights();
	TArray<TWeakObjectPtr<UAnomalyLightComponent>> AutoConnectedRectLights;
	
	// HotelPhoneActor의 C++ 이벤트에 연결합니다.
	// 일반 C++ 델리게이트이므로 UFUNCTION을 붙이지 않습니다.
	void HandleComplaintAnswered(const UObject* Requester);
	void HandleComplaintFinished(const UObject* Requester);

	// 아직 받지 않은 전화의 대사를 현재 조명 상태에 맞춰 갱신
	void RefreshComplaintMessage();

	UPROPERTY(Transient)
	TObjectPtr<UGameClockSubsystem> Clock;

	bool bStartedWithBlackout = false;
	bool bComplaintPending = false;
	double ComplaintDueSeconds = 0.0;
	
	bool bComplaintAnswered = false;
	bool bComplaintFinished = false;
};
