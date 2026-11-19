// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "CircuitBreakerActor.generated.h"

class ALightAnomaly;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BreakerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 복구 성공 시 BP에서 차단기 애니메이션과 소리를 재생합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Light")
	void OnBreakerUsed();
};