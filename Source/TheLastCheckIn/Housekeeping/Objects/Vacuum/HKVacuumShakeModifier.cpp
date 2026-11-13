#include "Housekeeping/Objects/Vacuum/HKVacuumShakeModifier.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

void UHKVacuumShakeModifier::Play(const APlayerController* PC, float InDuration, float InStrength)
{
	APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager)
	{
		return;
	}
	UHKVacuumShakeModifier* Modifier = Cast<UHKVacuumShakeModifier>(CameraManager->FindCameraModifierByClass(StaticClass()));
	if (!Modifier)
	{
		Modifier = Cast<UHKVacuumShakeModifier>(CameraManager->AddNewCameraModifier(StaticClass()));
	}
	if (Modifier)
	{
		Modifier->Elapsed = 0.0f;
		Modifier->Duration = FMath::Max(InDuration, KINDA_SMALL_NUMBER);
		Modifier->Strength = InStrength;
	}
}

bool UHKVacuumShakeModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	if (Strength <= 0.0f)
	{
		return false;
	}

	Elapsed += DeltaTime;
	NoiseTime += DeltaTime;

	/*
	흡입 진행에 따라 커지다가(끌려오는 힘) 끝나면 FadeOutTime 동안 줄어듦
	*/
	float Envelope;
	if (Elapsed < Duration)
	{
		const float Progress = Elapsed / Duration;
		Envelope = 0.35f + 0.65f * Progress * Progress;
	}
	else
	{
		Envelope = 1.0f - (Elapsed - Duration) / FadeOutTime;
	}
	if (Envelope <= 0.0f)
	{
		Strength = 0.0f;
		return false;
	}

	// 서로 다른 오프셋의 노이즈로 축마다 따로 흔듦
	constexpr float Frequency = 28.0f;
	const float Amount = Strength * Envelope * Alpha;
	const float Pitch = FMath::PerlinNoise1D(NoiseTime * Frequency) * Amount;
	const float Yaw = FMath::PerlinNoise1D(NoiseTime * Frequency + 37.1f) * Amount;
	const float Roll = FMath::PerlinNoise1D(NoiseTime * Frequency + 91.7f) * Amount * 0.5f;

	InOutPOV.Rotation += FRotator(Pitch, Yaw, Roll);
	return false;
}
