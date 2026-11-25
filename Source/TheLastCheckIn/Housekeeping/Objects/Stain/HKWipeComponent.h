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
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe", meta=(ClampMin=0, Units="cm"))
	float WallMargin = 10.0f;

	/*
	sound
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe|Sound")
	TObjectPtr<USoundBase> WipeLoopSound;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe|Sound", meta=(ClampMin=1, Units="cm/s"))
	float FullVolumeSpeed = 150.0f;

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
