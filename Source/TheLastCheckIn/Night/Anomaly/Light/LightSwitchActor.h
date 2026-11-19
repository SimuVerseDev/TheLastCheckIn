// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "LightSwitchActor.generated.h"

class ALightAnomaly;
class UStaticMeshComponent;
class UBoxComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SwitchMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 해결에 성공했을 때 BP에서 소리나 애니메이션을 재생합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Light")
	void OnSwitchUsed();
};