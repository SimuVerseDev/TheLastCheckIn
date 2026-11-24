#include "Night/Anomaly/Light/LightInteractionCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ULightInteractionCameraComponent::ULightInteractionCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULightInteractionCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>();
	if (Phase) Phase->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
}

bool ULightInteractionCameraComponent::CanStart(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	const APlayerController* PC = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	return !bActive && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
		&& PC && PC->IsLocalController() && PC->PlayerCameraManager
		&& PC->GetViewTarget() == Pawn && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored()
		&& (!Phase || Phase->GetCurrentPhase() != EHotelPhase::Settlement);
}

bool ULightInteractionCameraComponent::Start(AActor* Interactor, UCameraComponent* Camera, FSimpleDelegate Action)
{
	if (!CanStart(Interactor) || !IsValid(Camera) || !IsValid(Camera->GetOwner()) || !Action.IsBound()) return false;
	APawn* Pawn = CastChecked<APawn>(Interactor);
	APlayerController* PC = Pawn->GetController<APlayerController>();
	Controller = PC;
	InteractionPawn = Pawn;
	PreviousViewTarget = PC->GetViewTarget();
	FocusViewTarget = Camera->GetOwner();
	PendingAction = MoveTemp(Action);
	bActive = true;
	bWaitingForClick = false;
	bPreviousMouseCursor = PC->bShowMouseCursor;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
	InputBlocker = NewObject<UInputComponent>(this);
	InputBlocker->Priority = MAX_int32;
	InputBlocker->bBlockInput = true;
	InputBlocker->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::PerformAction);
	InputBlocker->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::Cancel);
	InputBlocker->RegisterComponent();
	PC->PushInputComponent(InputBlocker);

	Camera->Activate();
	const float Blend = FMath::Max(0.01f, CameraBlendTime);
	PC->SetViewTargetWithBlend(FocusViewTarget.Get(), Blend, VTBlend_EaseInOut, 2.0f);
	GetWorld()->GetTimerManager().SetTimer(SequenceTimer, this, &ThisClass::WaitForClick, Blend, false);
	return true;
}

bool ULightInteractionCameraComponent::StartLeverInteraction(AActor* Interactor, UCameraComponent* Camera,
	const TArray<UStaticMeshComponent*>& Levers, FLightLeverClick OnClick)
{
	if (Levers.IsEmpty() || !OnClick.IsBound()) return false;
	if (!Start(Interactor, Camera, FSimpleDelegate::CreateLambda([]() {}))) return false;
	for (UStaticMeshComponent* Lever : Levers) ClickableLevers.Add(Lever);
	LeverClick = MoveTemp(OnClick);
	Controller->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Controller->SetInputMode(Mode);
	return true;
}

bool ULightInteractionCameraComponent::SubmitLeverRay(const FVector& Start, const FVector& End)
{
	APlayerController* PC = Controller.Get();
	if (!bActive || !bWaitingForClick || !LeverClick.IsBound()) return false;
	if (!PC || !InteractionPawn.IsValid() || PC->GetPawn() != InteractionPawn.Get()
		|| !FocusViewTarget.IsValid() || PC->GetViewTarget() != FocusViewTarget.Get())
	{
		Cancel();
		return false;
	}
	UStaticMeshComponent* Selected = nullptr;
	float Nearest = 1.0f;
	// Test each lever's oriented local bounds, independent of the large E-interaction box.
	for (const auto& Weak : ClickableLevers)
	{
		UStaticMeshComponent* Mesh = Weak.Get();
		if (!Mesh || !Mesh->GetStaticMesh() || !Mesh->IsVisible()) continue;
		const FTransform Transform = Mesh->GetComponentTransform();
		FVector Hit, Normal;
		float Time;
		if (FMath::LineExtentBoxIntersection(Mesh->GetStaticMesh()->GetBoundingBox(),
			Transform.InverseTransformPosition(Start), Transform.InverseTransformPosition(End),
			FVector::ZeroVector, Hit, Normal, Time) && Time < Nearest)
		{
			Nearest = Time;
			Selected = Mesh;
		}
	}
	if (!Selected) return false;
	bWaitingForClick = false;
	const FLightLeverClick Callback = LeverClick;
	const bool bSolved = Callback.Execute(Selected);
	if (bActive)
	{
		if (bSolved) CompleteInteraction();
		else bWaitingForClick = true;
	}
	return true;
}

void ULightInteractionCameraComponent::WaitForClick()
{
	APlayerController* PC = Controller.Get();
	if (!bActive || !PC || !InteractionPawn.IsValid() || PC->GetPawn() != InteractionPawn.Get()
		|| !FocusViewTarget.IsValid() || PC->GetViewTarget() != FocusViewTarget.Get())
	{
		Cancel();
		return;
	}
	bWaitingForClick = true;
}

void ULightInteractionCameraComponent::PerformAction()
{
	// A click during the blend or after confirmation must not execute the action.
	if (!bActive || !bWaitingForClick) return;
	if (LeverClick.IsBound())
	{
		FVector Origin, Direction;
		if (Controller.IsValid() && Controller->DeprojectMousePositionToWorld(Origin, Direction))
			SubmitLeverRay(Origin, Origin + Direction * 10000.0f);
		return;
	}
	bWaitingForClick = false;
	APlayerController* PC = Controller.Get();
	if (!bActive || !PC || !InteractionPawn.IsValid() || PC->GetPawn() != InteractionPawn.Get()
		|| !FocusViewTarget.IsValid() || PC->GetViewTarget() != FocusViewTarget.Get()
		|| (Phase && Phase->GetCurrentPhase() == EHotelPhase::Settlement))
	{
		Cancel();
		return;
	}
	// Consume the action before executing it: Blueprint callbacks can cancel or destroy the actor.
	FSimpleDelegate Action = MoveTemp(PendingAction);
	PendingAction.Unbind();
	Action.ExecuteIfBound();
	if (!bActive) return;
	CompleteInteraction();
}

void ULightInteractionCameraComponent::CompleteInteraction()
{
	bWaitingForClick = false;
	GetWorld()->GetTimerManager().SetTimer(SequenceTimer, this, &ThisClass::ReturnToPlayer,
		FMath::Max(0.15f, LeverViewTime), false);
}

void ULightInteractionCameraComponent::ReturnToPlayer()
{
	APlayerController* PC = Controller.Get();
	AActor* Previous = PreviousViewTarget.Get();
	if (!bActive || !PC || !FocusViewTarget.IsValid() || PC->GetViewTarget() != FocusViewTarget.Get())
	{
		Cancel();
		return;
	}
	if (!Previous) Previous = PC->GetPawn();
	if (!Previous)
	{
		Finish();
		return;
	}
	const float Blend = FMath::Max(0.01f, CameraBlendTime);
	PC->SetViewTargetWithBlend(Previous, Blend, VTBlend_EaseInOut, 2.0f);
	GetWorld()->GetTimerManager().SetTimer(SequenceTimer, this, &ThisClass::Finish, Blend, false);
}

void ULightInteractionCameraComponent::Cancel()
{
	if (!bActive) return;
	if (APlayerController* PC = Controller.Get())
	{
		// Do not take the view back from another system (e.g. a new camera sequence).
		if (FocusViewTarget.IsValid() && PC->GetViewTarget() == FocusViewTarget.Get())
		{
			AActor* Previous = PreviousViewTarget.Get();
			if (!Previous) Previous = PC->GetPawn();
			if (Previous) PC->SetViewTargetWithBlend(Previous, 0.0f);
		}
	}
	Finish();
}

void ULightInteractionCameraComponent::Finish()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SequenceTimer);
	bWaitingForClick = false;
	PendingAction.Unbind();
	if (!bActive) return;
	bActive = false;
	if (APlayerController* PC = Controller.Get())
	{
		if (InputBlocker) PC->PopInputComponent(InputBlocker);
		if (LeverClick.IsBound()) PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = bPreviousMouseCursor;
		// Release only our own lock; settlement may have acquired another one meanwhile.
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	if (InputBlocker) InputBlocker->DestroyComponent();
	InputBlocker = nullptr;
	LeverClick.Unbind();
	ClickableLevers.Reset();
	Controller.Reset();
	InteractionPawn.Reset();
	PreviousViewTarget.Reset();
	FocusViewTarget.Reset();
}

void ULightInteractionCameraComponent::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase == EHotelPhase::Settlement) Cancel();
}

void ULightInteractionCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Cancel();
	if (Phase) Phase->OnPhaseChanged.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}
