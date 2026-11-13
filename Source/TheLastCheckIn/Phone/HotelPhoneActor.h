#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HotelPhoneActor.generated.h"

class UCameraComponent;
class UAudioComponent;
class USoundBase;

UCLASS()
class THELASTCHECKIN_API AHotelPhoneActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AHotelPhoneActor();
	
	UFUNCTION(BlueprintCallable, Category="Phone")
	void StartRinging(const FText& InSubtitle);
	
	UFUNCTION(BlueprintCallable, Category="Phone")
	void StopRinging();
	
	UFUNCTION(BlueprintPure, Category="Phone")
	bool IsRinging() const
	{
		return bRinging;
	}

	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	
protected:
	/*
	Fixed viewpoint
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Phone")
	TObjectPtr<UCameraComponent> ViewCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Phone")
	TObjectPtr<UAudioComponent> RingAudio;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Phone")
	TObjectPtr<USoundBase> RingSound;
	
	UPROPERTY(EditAnywhere, Category="Phone")
	FText AnswerPrompt;
	
private:
	FText PendingSubtitle;
	bool bRinging = false;
};