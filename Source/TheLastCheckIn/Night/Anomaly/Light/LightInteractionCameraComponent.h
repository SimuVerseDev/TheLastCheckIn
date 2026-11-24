#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "TimerManager.h"
#include "LightInteractionCameraComponent.generated.h"

class UCameraComponent;
class UInputComponent;
class APlayerController;
class APawn;
class UHotelPhaseSubsystem;
class UStaticMeshComponent;
DECLARE_DELEGATE_RetVal_OneParam(bool, FLightLeverClick, UStaticMeshComponent*);

/** Focus a camera, wait for a left click, then perform one interaction and restore the view. */
UCLASS()
class THELASTCHECKIN_API ULightInteractionCameraComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULightInteractionCameraComponent();
	bool CanStart(AActor* Interactor) const;
	bool Start(AActor* Interactor, UCameraComponent* Camera, FSimpleDelegate Action);
	void Cancel();
	bool StartLeverInteraction(AActor* Interactor, UCameraComponent* Camera,
		const TArray<UStaticMeshComponent*>& Levers, FLightLeverClick OnClick);
	// Shared cursor-ray path, also usable by input automation without a viewport.
	bool SubmitLeverRay(const FVector& Start, const FVector& End);

	UPROPERTY(EditAnywhere, Category = "Night|Camera", meta = (ClampMin = "0.01", Units = "s"))
	float CameraBlendTime = 0.3f;
	// Keep the lever visible long enough to see its existing 0.15-second animation.
	UPROPERTY(EditAnywhere, Category = "Night|Camera", meta = (ClampMin = "0.15", Units = "s"))
	float LeverViewTime = 0.35f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void WaitForClick();
	void CompleteInteraction();
	void PerformAction();
	void ReturnToPlayer();
	void Finish();
	UFUNCTION()
	void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);

	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<APawn> InteractionPawn;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	TWeakObjectPtr<AActor> FocusViewTarget;
	UPROPERTY(Transient) TObjectPtr<UInputComponent> InputBlocker;
	UPROPERTY(Transient) TObjectPtr<UHotelPhaseSubsystem> Phase;
	FSimpleDelegate PendingAction;
	FTimerHandle SequenceTimer;
	bool bActive = false;
	bool bWaitingForClick = false;
	bool bPreviousMouseCursor = false;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> ClickableLevers;
	FLightLeverClick LeverClick;
};
