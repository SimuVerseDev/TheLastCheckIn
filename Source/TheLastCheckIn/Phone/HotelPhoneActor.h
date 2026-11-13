#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HotelPhoneActor.generated.h"

class UCameraComponent;
class UAudioComponent;
class USoundBase;
class UPhoneSubtitleWidget;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/*
	Fixed viewpoint
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Phone")
	TObjectPtr<UCameraComponent> ViewCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Phone")
	TObjectPtr<UAudioComponent> RingAudio;
	
	UPROPERTY(EditAnywhere, Category="Phone")
	TObjectPtr<USoundBase> RingSound;
	
	UPROPERTY(EditAnywhere, Category="Phone")
	FText AnswerPrompt;
	
	UPROPERTY(EditAnywhere, Category="Phone", meta=(ClampMin="0.1", Units="s"))
	float CallDuration = 5.0f;
	
	UPROPERTY(EditAnywhere, Category="Phone", meta=(ClampMin="0.1", Units="s"))
	float CameraBlendTime = 0.5f;
	
private:
	void EndCall();
	
	TWeakObjectPtr<APlayerController> CallerPC;
	FTimerHandle CallTimer;
	
	/*
	UI
	*/
	UPROPERTY(Transient)
	TObjectPtr<UPhoneSubtitleWidget> SubtitleWidget;
	
	FText PendingSubtitle;
	bool bRinging = false;
};