// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HKTrashDisposalActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UHKTrashBinComponent;
class USoundBase;

UCLASS()
class THELASTCHECKIN_API AHKTrashDisposalActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHKTrashDisposalActor();
	
	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual bool CanUse_Implementation(
		AActor* Interactor) const override;

	virtual FText GetUsePrompt_Implementation(
		AActor* Interactor) const override;

	virtual bool Use_Implementation(
		AActor* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> DisposalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Housekeeping|Trash")
	FText EmptyPrompt;

	/*
	sound
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Trash|Sound")
	TObjectPtr<USoundBase> EmptySound;
	
	UFUNCTION(BlueprintImplementableEvent, Category="Housekeeping|Trash")
	void OnTrashEmptied(int32 RemovedAmount);

private:
	UHKTrashBinComponent* FindHeldTrashBin(AActor* Interactor) const;
};