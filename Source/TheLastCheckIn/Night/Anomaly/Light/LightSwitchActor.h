// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Night/Anomaly/Light/SwitchLeverAnimation.h"
#include "LightSwitchActor.generated.h"

class ALightAnomaly;
enum class EHotelLightMode : uint8;
class UStaticMeshComponent;
class UBoxComponent;
class UCameraComponent;
class ULightInteractionCameraComponent;

UCLASS()
class THELASTCHECKIN_API ALightSwitchActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ALightSwitchActor();

	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(
		AActor* Interactor) const override;

	virtual void Interact_Implementation(
		AActor* Interactor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	FName ZoneId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	TObjectPtr<ALightAnomaly> Anomaly;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SwitchMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 상호작용할 때 사용할 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULightInteractionCameraComponent> CameraSequence;

	// 해결에 성공했을 때 BP에서 소리나 애니메이션을 재생합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Light")
	void OnSwitchUsed();

private:
	// Local rotation from the lever's authored up position to its down position.
	UPROPERTY(EditDefaultsOnly, Category = "Night|Light")
	FRotator LeverDownRotation = FRotator(0.f, 0.f, 60.f);

	UFUNCTION()
	void HandleLightingChanged(EHotelLightMode Mode, FName AffectedZoneId);

	FSwitchLeverAnimation LeverAnimation;
};
