#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerInteractionComponent.generated.h"

class UInputAction;
class UEnhancedInputComponent;
class IInteractionExtension;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnUsed, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHKOnFocusChanged, AActor*, NewFocus, const FText&, Prompt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnHoldProgress, float, Alpha);
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerInteractionComponent();
	
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FHKOnFocusChanged OnFocusChanged;
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FHKOnHoldProgress OnHoldProgress;
	
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FHKOnUsed OnUsed;

	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetFocusedActor() const
	{
		return FocusedActor.Get();
	}

protected:
	UPROPERTY(EditAnywhere, Category="Interaction",  meta=(ClampMin=0, Units="cm"))
	float InteractRange = 250.0f;
	
	UPROPERTY(EditAnywhere, Category="Interaction",  meta=(ClampMin=0, Units="cm"))
	float TraceRadius = 8.0f;
	
	UPROPERTY(EditAnywhere, Category="Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> UseAction;
	
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bShowDebugPrompt = true;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	APlayerController* GetOwningPlayerController() const;
	
	static bool IsInteractable(AActor* Actor);
	bool CanFocus(AActor* Actor) const;

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
	TArray<IInteractionExtension*> GetExtensions() const;
	
	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> InteractionInput;
	TWeakObjectPtr<APlayerController> InputOwner;
	TWeakObjectPtr<AActor> FocusedActor;
	TWeakObjectPtr<AActor> HoldTarget;
	float HoldElapsed = 0.0f;
	float HoldDuration = 0.0f;
};