// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Night/Anomaly/Light/SwitchLeverAnimation.h"
#include "CircuitBreakerActor.generated.h"

class ALightAnomaly;
enum class EHotelLightMode : uint8;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class THELASTCHECKIN_API ACircuitBreakerActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACircuitBreakerActor();

	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(
		AActor* Interactor) const override;

	virtual void Interact_Implementation(
		AActor* Interactor) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	TObjectPtr<ALightAnomaly> Anomaly;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BreakerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 복구 성공 시 BP에서 차단기 애니메이션과 소리를 재생합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Light")
	void OnBreakerUsed();

private:
	// Local rotation from the lever's authored up position to its down position.
	UPROPERTY(EditDefaultsOnly, Category = "Night|Light")
	FRotator LeverDownRotation = FRotator(-60.f, 0.f, 0.f);

	UFUNCTION()
	void HandleLightingChanged(EHotelLightMode Mode, FName AffectedZoneId);

	FSwitchLeverAnimation LeverAnimation;
};