#include "Core/Interaction/PlayerInteractionComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "Core/Interaction/Interactable.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
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
void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveInteraction.Reset();
	SetFocus(nullptr);
	if (APlayerController* PC = InputOwner.Get())
	{
		PC->PopInputComponent(InteractionInput);
	}
	Super::EndPlay(EndPlayReason);
}

void UPlayerInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
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
APlayerController* UPlayerInteractionComponent::GetOwningPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetController<APlayerController>() : nullptr;
}

bool UPlayerInteractionComponent::IsInteractable(AActor* Actor)
{
	return Actor && Actor->Implements<UInteractable>();
}

bool UPlayerInteractionComponent::CanFocus(AActor* Actor) const
{
	if (!IsInteractable(Actor))
	{
		return false;
	}
	AActor* Pawn = GetOwner();
	return IInteractable::Execute_CanUse(Actor, Pawn) || IInteractable::Execute_CanInteract(Actor, Pawn);
}

void UPlayerInteractionComponent::EnsureInputSetup(APlayerController* PC)
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
			InteractionInput->BindAction(InteractAction, ETriggerEvent::Started, this, &UPlayerInteractionComponent::HandleInteractStarted);
		}
		if (UseAction)
		{
			InteractionInput->BindAction(UseAction, ETriggerEvent::Started, this, &UPlayerInteractionComponent::HandleUseStarted);
			InteractionInput->BindAction(UseAction, ETriggerEvent::Completed, this, &UPlayerInteractionComponent::HandleUseCompleted);
		}
	}
	PC->PushInputComponent(InteractionInput);
	InputOwner = PC;
}

void UPlayerInteractionComponent::UpdateFocus(APlayerController* PC)
{
	/*
	상호작용 중(아이템을 든 상태)이어도 시선 검사는 계속한다.
	E는 HandleInteractStarted에서 진행 중인 대상으로 가고, 사용(LMB) 가능한 대상만 포커스를 가져간다.
	*/
	AActor* Active = GetActiveInteraction();
	const TArray<IInteractionExtension*> Extensions = GetExtensions();
	if (!Active)
	{
		for (const IInteractionExtension* Extension : Extensions)
		{
			if (Extension->BlocksFocus())
			{
				SetFocus(nullptr);
				return;
			}
		}
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
	
	TArray<AActor*> IgnoredActors;
	for (const IInteractionExtension* Extension : Extensions)
	{
		Extension->GetIgnoredActors(IgnoredActors);
	}
	if (Active)
	{
		IgnoredActors.Add(Active); // 손에 든 아이템이 조준을 가리지 않도록
	}
	Params.AddIgnoredActors(IgnoredActors);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, ViewLocation, TraceEnd, FQuat::Identity, TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius), Params);
	AActor* Found = nullptr;
	AActor* OverlapTarget = nullptr;
	FVector RayEnd = TraceEnd;
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.bBlockingHit)
		{
			RayEnd = Hit.Location;
			if (CanFocus(Hit.GetActor()))
			{
				Found = Hit.GetActor();
				break;
			}
		}
		else if (!OverlapTarget && CanFocus(Hit.GetActor()))
		{
			// An overlapping cart body remains selectable without hiding the items it carries.
			// Multi traces stop at the first blocking hit, so targets behind walls stay excluded.
			OverlapTarget = Hit.GetActor();
		}
	}
	if (!Found)
	{
		Found = OverlapTarget;
	}

	if (!Found)
	{
		for (const IInteractionExtension* Extension : Extensions)
		{
			AActor* Target = Extension->FindInteractionTarget(ViewLocation, RayEnd);
			if (CanFocus(Target))
			{
				Found = Target;
				break;
			}
		}
	}

	if (Active)
	{
		SetFocus(Found && IInteractable::Execute_CanUse(Found, Pawn) ? Found : Active);
		return;
	}
	SetFocus(Found);
}

void UPlayerInteractionComponent::SetFocus(AActor* NewFocus)
{
	AActor* OldFocus = FocusedActor.Get();
	if (OldFocus == NewFocus)
	{
		return;
	}
	AActor* Pawn = GetOwner();
	if (IsInteractable(OldFocus))
	{
		IInteractable::Execute_OnInteractFocusEnd(OldFocus, Pawn);
	}
	FocusedActor = NewFocus;
	CancelHold();
	FText Prompt;
	if (IsInteractable(NewFocus))
	{
		IInteractable::Execute_OnInteractFocusBegin(NewFocus, Pawn);

		Prompt = IInteractable::Execute_CanUse(NewFocus, Pawn)
			? IInteractable::Execute_GetUsePrompt(NewFocus, Pawn)
			: IInteractable::Execute_GetInteractPrompt(NewFocus, Pawn);
	}
	OnFocusChanged.Broadcast(NewFocus, Prompt);
}

void UPlayerInteractionComponent::UpdateHold(float DeltaTime)
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
		ExecuteInteraction(Target);
	}
}

void UPlayerInteractionComponent::CancelHold()
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
void UPlayerInteractionComponent::ShowDebugPrompt() const
{
	if (!GEngine)
	{
		return;
	}
	FString Message;
	for (const IInteractionExtension* Extension : GetExtensions())
	{
		if (Extension->BlocksFocus())
		{
			Message = Extension->GetBlockingPrompt().ToString();
			break;
		}
	}
	if (AActor* Target = FocusedActor.Get(); Message.IsEmpty() && IsInteractable(Target))
	{
		AActor* Pawn = GetOwner();
		if (IInteractable::Execute_CanUse(Target, Pawn))
		{
			Message = FString::Printf(TEXT("[LMB] %s"), *IInteractable::Execute_GetUsePrompt(Target, Pawn).ToString());
		}
		if (IInteractable::Execute_CanInteract(Target, Pawn))
		{
			if (!Message.IsEmpty())
			{
				Message += TEXT("\n");
			}
			Message += FString::Printf(TEXT("[E] %s"), *IInteractable::Execute_GetInteractPrompt(Target, Pawn).ToString());
		}
	}
	if (!Message.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow, Message);
	}
}


void UPlayerInteractionComponent::HandleInteractStarted()
{
	AActor* Pawn = GetOwner();
	AActor* Target = GetActiveInteraction();
	if (!Target)
	{
		Target = FocusedActor.Get();
	}
	if (!IsInteractable(Target) || !IInteractable::Execute_CanInteract(Target, Pawn))
	{
		return;
	}
	const float Duration = IInteractable::Execute_GetInteractHoldDuration(Target, Pawn);
	if (Duration <= 0.0f)
	{
		ExecuteInteraction(Target);
	}
	else
	{
		HoldTarget = Target;
		HoldDuration = Duration;
		HoldElapsed = 0.0f;
	}
}

AActor* UPlayerInteractionComponent::GetActiveInteraction()
{
	AActor* Target = ActiveInteraction.Get();
	if (IsValid(Target) && IsInteractable(Target)
		&& IInteractable::Execute_IsInteractionActive(Target, GetOwner()))
	{
		return Target;
	}
	ActiveInteraction.Reset();
	return nullptr;
}

void UPlayerInteractionComponent::ExecuteInteraction(AActor* Target)
{
	IInteractable::Execute_Interact(Target, GetOwner());
	ActiveInteraction = IsValid(Target)
		&& IInteractable::Execute_IsInteractionActive(Target, GetOwner()) ? Target : nullptr;
}

void UPlayerInteractionComponent::HandleUseStarted()
{
	AActor* Pawn = GetOwner();
	AActor* Target = FocusedActor.Get();
	if (!IsInteractable(Target) || !IInteractable::Execute_CanUse(Target, Pawn))
	{
		return;
	}
	if (IInteractable::Execute_Use(Target, Pawn))
	{
		OnUsed.Broadcast(Target);
		SetFocus(nullptr);
	}
}

void UPlayerInteractionComponent::HandleUseCompleted()
{
	for (IInteractionExtension* Extension : GetExtensions())
	{
		Extension->OnUseReleased();
	}
}

bool UPlayerInteractionComponent::IsActionKeyDown(const UInputAction* Action) const
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

TArray<IInteractionExtension*> UPlayerInteractionComponent::GetExtensions() const
{
	TArray<IInteractionExtension*> Extensions;
	for (UActorComponent* Component : GetOwner()->GetComponents())
	{
		if (IInteractionExtension* Extension = Cast<IInteractionExtension>(Component))
		{
			Extensions.Add(Extension);
		}
	}
	return Extensions;
}
