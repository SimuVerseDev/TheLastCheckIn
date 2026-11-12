#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "VacuumToolActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class THELASTCHECKIN_API AVacuumToolActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AVacuumToolActor();
	
	static AVacuumToolActor* FindHeldBy(const AActor* Interactor);

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vacuum")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/*
	흡입구
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vacuum")
	TObjectPtr<USceneComponent> Nozzle;


	UPROPERTY(EditAnywhere, Category="Vacuum")
	FTransform HeldOffset = FTransform(FRotator::ZeroRotator, FVector(60.0f, 25.0f, -40.0f));

	// HKBlanketActor.h - FText GrabPrompt
	UPROPERTY(EditAnywhere, Category="Vacuum")
	FText PickupPrompt;
	UPROPERTY(EditAnywhere, Category="Vacuum")
	FText DropPrompt;

private:
	void Equip(AActor* Interactor);
	void Unequip();

	TWeakObjectPtr<AActor> Holder;
};