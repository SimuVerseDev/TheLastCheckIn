#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HKVacuumToolActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class THELASTCHECKIN_API AHKVacuumToolActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AHKVacuumToolActor();
	
	static AHKVacuumToolActor* FindHeldBy(const AActor* Interactor);

	USceneComponent* GetNozzle() const
	{
		return Nozzle;
	}

	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Housekeeping|Vacuum")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/*
	흡입구
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Housekeeping|Vacuum")
	TObjectPtr<USceneComponent> Nozzle;


	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum")
	FTransform HeldOffset = FTransform(FRotator::ZeroRotator, FVector(60.0f, 25.0f, -40.0f));

	// HKBlanketActor.h - FText GrabPrompt
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum")
	FText PickupPrompt;
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum")
	FText DropPrompt;

private:
	void Equip(AActor* Interactor);
	void Unequip();

	TWeakObjectPtr<AActor> Holder;
};