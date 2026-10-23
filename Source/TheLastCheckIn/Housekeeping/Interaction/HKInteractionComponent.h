#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HKInteractionComponent.generated.h"

class UInputAction;
class UEnhancedInputComponent;
class UHKCarryComponent;
class AHKPhysicsPickupActor;
class AHKBlanketActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHKOnFocusChanged, AActor*, NewFocus, const FText&, Prompt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnHoldProgress, float, Alpha);
UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHKInteractionComponent();
	
	UPROPERTY(BlueprintAssignable, Category="Housekeeping|Interaction")
	FHKOnFocusChanged OnFocusChanged;
	UPROPERTY(BlueprintAssignable, Category="Housekeeping|Interaction")
	FHKOnHoldProgress OnHoldProgress;
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Interaction")
	AActor* GetFocusedActor() const
	{
		return FocusedActor.Get();
	}
	
protected:
	UPROPERTY(EditAnywhere, Category="Housekeeping|Interaction",  meta=(ClampMin=0, Units="cm"))
	float InteractRange = 250.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Interaction",  meta=(ClampMin=0, Units="cm"))
	float TraceRadius = 8.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Interaction", meta=(ClampMin=0, Units="cm"))
	float BlanketGrabRadius = 30.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Input")
	TObjectPtr<UInputAction> UseAction;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Debug")
	bool bShowDebugPrompt = true;
	
	/*
	임시 카트모드 전환용
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Housekeeping|Debug")
	bool bDebugTrashMode = false;

	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	APlayerController* GetOwningPlayerController() const;
	UHKCarryComponent* GetCarryComponent() const;
	AHKPhysicsPickupActor* GetFocusedPickup() const;
	AHKBlanketActor* GetFocusedBlanket() const;
	AHKBlanketActor* FindBlanketOnRay(const FVector& Start, const FVector& End) const;

	static bool IsInteractable(AActor* Actor);

	void EnsureInputSetup(APlayerController* PC);
	void UpdateFocus(APlayerController* PC);
	void SetFocus(AActor* NewFocus);
	void UpdateHold(float DeltaTime);
	void CancelHold();
	void ShowDebugPrompt() const;
	void HandleInteractStarted();
	void HandleUseStarted();
	void HandleUseCompleted();

	bool IsActionKeyDown(const UInputAction* Action) const;
	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InteractionInput;
	TWeakObjectPtr<APlayerController> InputOwner;
	TWeakObjectPtr<AActor> FocusedActor;
	TWeakObjectPtr<AActor> HoldTarget;
	float HoldElapsed = 0.0f;
	float HoldDuration = 0.0f;
};