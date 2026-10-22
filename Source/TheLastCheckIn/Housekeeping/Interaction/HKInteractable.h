#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HKInteractable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UHKInteractable : public UInterface
{
	GENERATED_BODY()
};

class IHKInteractable
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Housekeeping|Interaction")
	bool CanInteract(AActor* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Housekeeping|Interaction")
	FText GetInteractPrompt(AActor* Interactor) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Housekeeping|Interaction")
	float GetInteractHoldDuration(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Housekeeping|Interaction")
	void Interact(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Housekeeping|Interaction")
	void OnInteractFocusBegin(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Housekeeping|Interaction")
	void OnInteractFocusEnd(AActor* Interactor);
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const;
	virtual float GetInteractHoldDuration_Implementation(AActor* Interactor) const;
	virtual void Interact_Implementation(AActor* Interactor);
	virtual void OnInteractFocusBegin_Implementation(AActor* Interactor);
	virtual void OnInteractFocusEnd_Implementation(AActor* Interactor);
};
