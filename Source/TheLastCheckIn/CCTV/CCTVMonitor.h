#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "CCTVMonitor.generated.h"

class ACameraActor;
class APlayerController;
class UCCTVPadWidget;
class UStaticMeshComponent;

UCLASS()
class THELASTCHECKIN_API ACCTVMonitor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ACCTVMonitor();
	/*
	Interactable override
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;
	
protected:
	UPROPERTY(VisibleAnywhere, Category="CCTV")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditDefaultsOnly, Category="CCTV")
	TSubclassOf<UCCTVPadWidget> PadWidgetClass;

	UPROPERTY(EditInstanceOnly, Category="CCTV")
	TArray<TObjectPtr<ACameraActor>> Cameras;
	
private:
	void OpenPad(APlayerController* PlayerController);
	void ClosePad();
	
	UFUNCTION()
	void HandlePadCovered();
	
	UFUNCTION()
	void HandlePadLowered();
	
	UPROPERTY()
	TObjectPtr<UCCTVPadWidget> PadWidget;
	TWeakObjectPtr<APlayerController> ViewingPC;
	
	int32 CurrentCameraIndex = 0;
	bool bPadOpen = false;
	bool bTransitioning = false;
};