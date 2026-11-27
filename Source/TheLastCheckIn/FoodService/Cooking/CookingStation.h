#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "CookingStation.generated.h"

class AFoodItemActor;
class USceneComponent;
class UStaticMeshComponent;
class UFoodSnapSlotComponent;

UCLASS()
class THELASTCHECKIN_API ACookingStation
	: public AActor
	, public IInteractable
{
	GENERATED_BODY()

public:
	ACookingStation();

	/*
	Interact : E키
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	virtual bool CanInteractWhileCarrying_Implementation(AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;

	// 좌클릭으로 완성된 음식 꺼내기
	virtual bool CanUse_Implementation(AActor* Interactor) const override;
	virtual FText GetUsePrompt_Implementation(AActor* Interactor) const override;
	virtual bool Use_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food|Cooking")
	TObjectPtr<UFoodSnapSlotComponent> CookingSlot;

private:
	UFUNCTION()
	void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

	void CancelCurrentCooking();

	UPROPERTY(Transient)
	TWeakObjectPtr<AFoodItemActor> CookingFood;

	FGuid CookingLease;
	double FinishAtGameSeconds = -1.0;
};