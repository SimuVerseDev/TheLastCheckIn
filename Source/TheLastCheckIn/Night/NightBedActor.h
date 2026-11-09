#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "NightBedActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class THELASTCHECKIN_API ANightBedActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ANightBedActor();
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual float GetInteractHoldDuration_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool CanUse_Implementation(AActor* Interactor) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bed")
	TObjectPtr<UStaticMeshComponent> BedMesh;
};
