#include "Housekeeping/Interaction/HKInteractionComponent.h"
#include "Housekeeping/Interaction/HKInteractable.h"
#include "Housekeeping/Carry/HKCarryComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Housekeeping/Carry/PickupActor/Blanket/HKBlanketActor.h"
#include "EngineUtils.h"

UHKInteractionComponent::UHKInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UInputAction> InteractFinder(TEXT("/Game/Input/Actions/IA_Interact.IA_Interact"));
	if (InteractFinder.Succeeded())
	{
		InteractAction = InteractFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> UseFinder(TEXT("/Game/Input/Actions/IA_Use.IA_Use"));
	if (UseFinder.Succeeded())
	{
		UseAction = UseFinder.Object;
	}
}

/*
Virtual 
*/
void UHKInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetFocus(nullptr);
	if (APlayerController* PC = InputOwner.Get())
	{
		PC->PopInputComponent(InteractionInput);
	}
	Super::EndPlay(EndPlayReason);
}

void UHKInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	EnsureInputSetup(PC);
	UpdateFocus(PC);
	UpdateHold(DeltaTime);
	if (bShowDebugPrompt)
	{
		ShowDebugPrompt();
	}
}

/*
private
*/
APlayerController* UHKInteractionComponent::GetOwningPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetController<APlayerController>() : nullptr;
}

UHKCarryComponent* UHKInteractionComponent::GetCarryComponent() const
{
	return GetOwner()->FindComponentByClass<UHKCarryComponent>();
}

/*
Blanket
*/
AHKBlanketActor* UHKInteractionComponent::FindBlanketOnRay(const FVector& Start, const FVector& End) const
{
	for (TActorIterator<AHKBlanketActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsGrabbed() && It->FindParticleNearRay(Start, End, BlanketGrabRadius) != INDEX_NONE)
		{
			return *It;
		}
	}
	return nullptr;
}

/*
Trash
*/
USceneComponent* UHKInteractionComponent::GetTrashAbsorbTarget() const
{
	if (USceneComponent* Target = TrashAbsorbTarget.Get())
	{
		return Target;
	}
	return bDebugTrashMode ? GetOwner()->GetRootComponent() : nullptr;
}

bool UHKInteractionComponent::IsInteractable(AActor* Actor)
{
	return Actor && Actor->Implements<UHKInteractable>();
}

bool UHKInteractionComponent::CanFocus(AActor* Actor) const
{
	if (!IsInteractable(Actor))
	{
		return false;
	}
	AActor* Pawn = GetOwner();
	return IHKInteractable::Execute_CanUse(Actor, Pawn) || IHKInteractable::Execute_CanInteract(Actor, Pawn);
}

void UHKInteractionComponent::EnsureInputSetup(APlayerController* PC)
{
	if (InputOwner.Get() == PC)
	{
		return;
	}
	if (APlayerController* OldPC = InputOwner.Get())
	{
		OldPC->PopInputComponent(InteractionInput);
	}
	if (!InteractionInput)
	{
		InteractionInput = NewObject<UEnhancedInputComponent>(GetOwner(), TEXT("HKInteractionInput"));
		InteractionInput->RegisterComponent();
		if (InteractAction)
		{
			InteractionInput->BindAction(InteractAction, ETriggerEvent::Started, this, &UHKInteractionComponent::HandleInteractStarted);
		}
		if (UseAction)
		{
			InteractionInput->BindAction(UseAction, ETriggerEvent::Started, this, &UHKInteractionComponent::HandleUseStarted);
			InteractionInput->BindAction(UseAction, ETriggerEvent::Completed, this, &UHKInteractionComponent::HandleUseCompleted);
		}
	}
	PC->PushInputComponent(InteractionInput);
	InputOwner = PC;
}

void UHKInteractionComponent::UpdateFocus(APlayerController* PC)
{
	const UHKCarryComponent* Carry = GetCarryComponent();
	if (Carry && Carry->IsCarrying())
	{
		SetFocus(nullptr);
		return;
	}
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	
	/*
	3인칭이면 카메라가 폰 뒤에 있으니 그만큼 거리를 증가
	*/
	const float CameraOffset = FVector::Dist(ViewLocation, Pawn->GetPawnViewLocation());
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * (InteractRange + CameraOffset);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HKInteractTrace), false, Pawn);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, ViewLocation, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TraceRadius), Params);
	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (CanFocus(HitActor))
	{
		SetFocus(HitActor);
	}
	else if (AHKBlanketActor* Blanket = FindBlanketOnRay(ViewLocation, bHit ? Hit.Location : TraceEnd); CanFocus(Blanket))
	{
		SetFocus(Blanket);
	}
	else
	{
		SetFocus(nullptr);
	}
}

void UHKInteractionComponent::SetFocus(AActor* NewFocus)
{
	AActor* OldFocus = FocusedActor.Get();
	if (OldFocus == NewFocus)
	{
		return;
	}
	AActor* Pawn = GetOwner();
	if (IsInteractable(OldFocus))
	{
		IHKInteractable::Execute_OnInteractFocusEnd(OldFocus, Pawn);
	}
	FocusedActor = NewFocus;
	CancelHold();
	FText Prompt;
	if (IsInteractable(NewFocus))
	{
		IHKInteractable::Execute_OnInteractFocusBegin(NewFocus, Pawn);

		Prompt = IHKInteractable::Execute_CanUse(NewFocus, Pawn)
			? IHKInteractable::Execute_GetUsePrompt(NewFocus, Pawn)
			: IHKInteractable::Execute_GetInteractPrompt(NewFocus, Pawn);
	}
	OnFocusChanged.Broadcast(NewFocus, Prompt);
}

void UHKInteractionComponent::UpdateHold(float DeltaTime)
{
	AActor* Target = HoldTarget.Get();
	if (!Target)
	{
		return;
	}
	if (Target != FocusedActor.Get() /*시선이 벗어났거나*/ || !IsActionKeyDown(InteractAction) /*키를 뗐으면*/)
	{
		CancelHold();
		return;
	}
	HoldElapsed += DeltaTime;
	OnHoldProgress.Broadcast(FMath::Clamp(HoldElapsed / HoldDuration, 0.0f, 1.0f));
	if (HoldElapsed >= HoldDuration)
	{
		HoldTarget.Reset();
		IHKInteractable::Execute_Interact(Target, GetOwner());
	}
}

void UHKInteractionComponent::CancelHold()
{
	if (HoldTarget.IsValid())
	{
		HoldTarget.Reset();
		OnHoldProgress.Broadcast(0.0f);
	}
	HoldElapsed = 0.0f;
}

/*
DEBUG
*/
void UHKInteractionComponent::ShowDebugPrompt() const
{
	if (!GEngine)
	{
		return;
	}
	FString Message;
	const UHKCarryComponent* Carry = GetCarryComponent();
	if (Carry && Carry->IsCarrying())
	{
		Message = TEXT("[LMB 떼기] 놓기");
	}
	else if (AActor* Target = FocusedActor.Get(); IsInteractable(Target))
	{
		AActor* Pawn = GetOwner();
		if (IHKInteractable::Execute_CanUse(Target, Pawn))
		{
			Message = FString::Printf(TEXT("[LMB] %s"), *IHKInteractable::Execute_GetUsePrompt(Target, Pawn).ToString());
		}
		if (IHKInteractable::Execute_CanInteract(Target, Pawn))
		{
			if (!Message.IsEmpty())
			{
				Message += TEXT("\n");
			}
			Message += FString::Printf(TEXT("[E] %s"), *IHKInteractable::Execute_GetInteractPrompt(Target, Pawn).ToString());
		}
	}
	if (!Message.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow, Message);
	}
}


void UHKInteractionComponent::HandleInteractStarted()
{
	AActor* Pawn = GetOwner();
	AActor* Target = FocusedActor.Get();
	if (!IsInteractable(Target) || !IHKInteractable::Execute_CanInteract(Target, Pawn))
	{
		return;
	}
	const float Duration = IHKInteractable::Execute_GetInteractHoldDuration(Target, Pawn);
	if (Duration <= 0.0f)
	{
		IHKInteractable::Execute_Interact(Target, Pawn);
	}
	else
	{
		HoldTarget = Target;
		HoldDuration = Duration;
		HoldElapsed = 0.0f;
	}
}

void UHKInteractionComponent::HandleUseStarted()
{
	AActor* Pawn = GetOwner();
	AActor* Target = FocusedActor.Get();
	if (!IsInteractable(Target) || !IHKInteractable::Execute_CanUse(Target, Pawn))
	{
		return;
	}
	if (IHKInteractable::Execute_Use(Target, Pawn))
	{
		OnUsed.Broadcast(Target);
		SetFocus(nullptr);
	}
}

void UHKInteractionComponent::HandleUseCompleted()
{
	if (UHKCarryComponent* Carry = GetCarryComponent())
	{
		Carry->Drop();
	}
}

bool UHKInteractionComponent::IsActionKeyDown(const UInputAction* Action) const
{
	APlayerController* PC = InputOwner.Get();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!Action || !LocalPlayer)
	{
		return false;
	}
	if (const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		for (const FKey& Key : Subsystem->QueryKeysMappedToAction(Action))
		{
			if (PC->IsInputKeyDown(Key))
			{
				return true;
			}
		}
	}
	return false;
}
