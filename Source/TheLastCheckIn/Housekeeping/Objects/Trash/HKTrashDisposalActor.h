// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HKTrashDisposalActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UHKTrashBinComponent;

UCLASS()
class THELASTCHECKIN_API AHKTrashDisposalActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHKTrashDisposalActor();

	// E 상호작용 대신 Use(좌클릭)를 사용합니다.
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

	// 비우기에 성공했을 때 BP에서 소리나 효과를 재생할 수 있습니다.
	UFUNCTION(BlueprintImplementableEvent, Category="Housekeeping|Trash")
	void OnTrashEmptied(int32 RemovedAmount);

private:
	// 플레이어의 물리 핸들이 실제로 들고 있는 쓰레기통을 찾습니다.
	UHKTrashBinComponent* FindHeldTrashBin(AActor* Interactor) const;
};