#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "HKWipeComponent.generated.h"
class AHKStainActor;
class APlayerController;
class UHKWipeToolComponent;
class UPrimitiveComponent;
class USoundBase;
class UAudioComponent;

UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UHKWipeComponent : public UActorComponent, public IInteractionExtension
{
	GENERATED_BODY()

public:
	UHKWipeComponent();

	bool BeginWipe(AHKStainActor* InStain, UHKWipeToolComponent* InTool);
	void EndWipe();

	UFUNCTION(BlueprintPure, Category="Housekeeping|Wipe")
	bool IsWiping() const
	{
		return bWiping;
	}

	virtual bool BlocksFocus() const override
	{
		return IsWiping();
	}
	virtual void OnUseReleased() override
	{
		EndWipe();
	}

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe", meta=(ClampMin=0, Units="cm"))
	float MouseSensitivity = 2.0f;

	// 닦는 동안 반복 재생 (Looping 사운드), 마우스를 움직일 때만 들림
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe|Sound")
	TObjectPtr<USoundBase> WipeLoopSound;

	// 이 속도(cm/s) 이상 문지르면 최대 볼륨
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe|Sound", meta=(ClampMin=1, Units="cm/s"))
	float FullVolumeSpeed = 150.0f;

	// 볼륨이 목표값을 따라가는 속도 (클수록 즉각 반응)
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe|Sound", meta=(ClampMin=0.1))
	float VolumeInterpSpeed = 12.0f;

private:
	void StopWipeSound();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WipeAudio;
	float WipeVolume = 0.0f;

	UPrimitiveComponent* GetToolBody() const;
	APlayerController* GetPlayerController() const;

	TWeakObjectPtr<AHKStainActor> Stain;
	TWeakObjectPtr<UHKWipeToolComponent> Tool;
	TWeakObjectPtr<APlayerController> LookLockedPC;
	bool bWiping = false;
	FVector2D WipeOffset = FVector2D::ZeroVector;
};
