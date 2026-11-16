#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "KnockDoorActor.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class ARoomActor;
class AKnockDoorActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKnockDoorOpenChanged, AKnockDoorActor*, Door, bool, bOpen);

UCLASS()
class THELASTCHECKIN_API AKnockDoorActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AKnockDoorActor();
	virtual void Tick(float DeltaSeconds) override;
	
	UFUNCTION(BlueprintCallable, Category = "Night|Knock")
	void SetOpen(bool bInOpen);
	
	UFUNCTION(BlueprintPure, Category="Night|Knock")
	bool IsOpen() const
	{
		return bOpen;
	}

	UFUNCTION(BlueprintPure, Category="Night|Knock")
	ARoomActor* GetRoom() const
	{
		return Room;
	}

	UFUNCTION(BlueprintCallable, Category="Night|Knock")
	void PlayKnock();

	UFUNCTION(BlueprintCallable, Category="Night|Knock")
	void StopKnock();

	UPROPERTY(BlueprintAssignable, Category="Night|Knock")
	FKnockDoorOpenChanged OnOpenChanged;
	
	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAudioComponent> KnockAudio;
	
	/* 
	CCTV실·직원실 = true, 객실 = false
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Knock")
	bool bCanOpen = true;

	/*
	객실 문일 때만 지정
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Knock", meta=(EditCondition="!bCanOpen"))
	TObjectPtr<ARoomActor> Room;

	UPROPERTY(EditAnywhere, Category="Night|Knock")
	TObjectPtr<USoundBase> KnockSound;

	UPROPERTY(EditAnywhere, Category="Night|Knock")
	float OpenYaw = 90.0f;

	/*
	초당 회전 각도
	*/
	UPROPERTY(EditAnywhere, Category="Night|Knock", meta=(ClampMin="1"))
	float OpenSpeed = 180.0f;

	UPROPERTY(EditAnywhere, Category="Night|Knock")
	FText OpenPrompt;

	UPROPERTY(EditAnywhere, Category="Night|Knock")
	FText ClosePrompt;
	
private:
	bool bOpen = false;
	float CurrentYaw = 0.0f;
};