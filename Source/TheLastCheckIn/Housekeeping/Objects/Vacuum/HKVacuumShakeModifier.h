#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "HKVacuumShakeModifier.generated.h"

/*
청소기 흡입 중 카메라 진동 - 흡입이 진행될수록 세지고 끝나면 짧게 잦아듦
엔진 카메라 셰이크 플러그인 없이 펄린 노이즈로 시점만 살짝 흔든다
*/
UCLASS()
class THELASTCHECKIN_API UHKVacuumShakeModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	static void Play(const APlayerController* PC, float InDuration, float InStrength);

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;

private:
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	float Strength = 0.0f;
	float NoiseTime = 0.0f;

	// 흡입이 끝난 뒤 진동이 잦아드는 시간
	static constexpr float FadeOutTime = 0.2f;
};
