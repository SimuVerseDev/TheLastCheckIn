#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "CCTVMonitor.generated.h"

class ACameraActor;
class APlayerController;
class UCCTVPadWidget;
class UStaticMeshComponent;

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
struct FInputActionValue;

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
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, Category="CCTV")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditDefaultsOnly, Category="CCTV")
	TSubclassOf<UCCTVPadWidget> PadWidgetClass;

	UPROPERTY(EditInstanceOnly, Category="CCTV")
	TArray<TObjectPtr<ACameraActor>> Cameras;
	
	UPROPERTY(EditDefaultsOnly, Category="CCTV|Input")
	TObjectPtr<UInputMappingContext> CCTVMappingContext;

	/*
	Axis1D : → = +1(다음), ← = -1(이전) 
	*/
	UPROPERTY(EditDefaultsOnly, Category="CCTV|Input")
	TObjectPtr<UInputAction> SwitchCameraAction;
	
	UPROPERTY(EditDefaultsOnly, Category="CCTV|Input")
	int32 CCTVMappingPriority = 10;
	
private:
	void OpenPad(APlayerController* PlayerController);
	void ClosePad();
	void BindCCTVInput(APlayerController* PC);
	void UnbindCCTVInput();
	void HandleSwitchCamera(const FInputActionValue& Value);
	void SwitchCamera(int32 Direction);
	void ApplyCurrentCamera() const;

	
	UFUNCTION()
	void HandlePadCovered();
	
	UFUNCTION()
	void HandlePadLowered();
	
	UPROPERTY()
	TObjectPtr<UCCTVPadWidget> PadWidget;
	TWeakObjectPtr<APlayerController> ViewingPC;
	
	UPROPERTY()
	TObjectPtr<UEnhancedInputComponent> CCTVInput;
	TWeakObjectPtr<APlayerController> InputPC;
	
	int32 CurrentCameraIndex = 0;
	bool bPadOpen = false;
	bool bTransitioning = false;
};