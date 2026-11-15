#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "DesktopActor.generated.h"

class UDesktopWidget;

UCLASS()
class THELASTCHECKIN_API ADesktopActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Desktop")
	TSubclassOf<UDesktopWidget> DesktopWidgetClass;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDesktopWidget> OpenWidget;
};
