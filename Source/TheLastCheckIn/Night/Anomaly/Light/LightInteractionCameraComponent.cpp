#include "Night/Anomaly/Light/LightInteractionCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"

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
	if (!CanStart(Interactor) || !IsValid(Camera) || Camera->GetOwner() != GetOwner() || !Action.IsBound()) return false;
	APawn* Pawn = CastChecked<APawn>(Interactor);
	APlayerController* PC = Pawn->GetController<APlayerController>();
	Controller = PC;
	InteractionPawn = Pawn;
	PreviousViewTarget = PC->GetViewTarget();
	PendingAction = MoveTemp(Action);
	bActive = true;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
	InputBlocker = NewObject<UInputComponent>(this);
	InputBlocker->Priority = MAX_int32;
	InputBlocker->bBlockInput = true;
	InputBlocker->RegisterComponent();
	PC->PushInputComponent(InputBlocker);

	Camera->Activate();
	const float Blend = FMath::Max(0.01f, CameraBlendTime);
	PC->SetViewTargetWithBlend(GetOwner(), Blend, VTBlend_EaseInOut, 2.0f);
	GetWorld()->GetTimerManager().SetTimer(SequenceTimer, this, &ThisClass::PerformAction, Blend, false);
	return true;
}

void ULightInteractionCameraComponent::PerformAction()
{
	APlayerController* PC = Controller.Get();
	if (!bActive || !PC || !InteractionPawn.IsValid() || PC->GetPawn() != InteractionPawn.Get()
		|| PC->GetViewTarget() != GetOwner() || (Phase && Phase->GetCurrentPhase() == EHotelPhase::Settlement))
	{
		Cancel();
		return;
	}
	// Consume the action before executing it: Blueprint callbacks can cancel or destroy the actor.
	FSimpleDelegate Action = MoveTemp(PendingAction);
	PendingAction.Unbind();
	Action.ExecuteIfBound();
	if (!bActive) return;
	GetWorld()->GetTimerManager().SetTimer(SequenceTimer, this, &ThisClass::ReturnToPlayer,
		FMath::Max(0.15f, LeverViewTime), false);
}

void ULightInteractionCameraComponent::ReturnToPlayer()
{
	APlayerController* PC = Controller.Get();
	AActor* Previous = PreviousViewTarget.Get();
	if (!bActive || !PC || PC->GetViewTarget() != GetOwner())
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
		if (PC->GetViewTarget() == GetOwner())
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
	PendingAction.Unbind();
	if (!bActive) return;
	bActive = false;
	if (APlayerController* PC = Controller.Get())
	{
		if (InputBlocker) PC->PopInputComponent(InputBlocker);
		// Release only our own lock; settlement may have acquired another one meanwhile.
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	if (InputBlocker) InputBlocker->DestroyComponent();
	InputBlocker = nullptr;
	Controller.Reset();
	InteractionPawn.Reset();
	PreviousViewTarget.Reset();
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
