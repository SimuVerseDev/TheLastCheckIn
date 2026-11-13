#include "Night/NightFlowComponent.h"
#include "Night/NightBedActor.h"
#include "Night/Settlement/DailySettlementSubsystem.h"
#include "Night/Settlement/SettlementWidget.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/GameFlow/HotelPhaseSettings.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Tutorial/TutorialSubsystem.h"
#include "Core/Interaction/PlayerInteractionComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogNightFlow, Log, All);

void UNightBlackoutWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BlackBackground"));
	Background->SetBrushColor(FLinearColor::Black);
	WidgetTree->RootWidget = Background;
}

UNightFlowComponent::UNightFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Read the interaction component's completed focus result; do not run another trace.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SettlementWidgetClass = USettlementWidget::StaticClass();
}

void UNightSleepPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	Background->SetPadding(FMargin(18.0f, 10.0f));
	UCanvasPanelSlot* PromptSlot = Canvas->AddChildToCanvas(Background);
	PromptSlot->SetAnchors(FAnchors(0.5f, 0.58f));
	PromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PromptSlot->SetPosition(FVector2D::ZeroVector);
	PromptSlot->SetAutoSize(true);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SleepPromptText"));
	Label->SetText(NSLOCTEXT("Night", "SleepKeyPrompt", "[E 잠자기]"));
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 24));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetJustification(ETextJustify::Center);
	Background->SetContent(Label);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

UNightFlowComponent* UNightFlowComponent::FindForActor(AActor* Actor)
{
	if (!IsValid(Actor)) return nullptr;
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (AController* Controller = Pawn->GetController())
		{
			if (UNightFlowComponent* Flow = Controller->FindComponentByClass<UNightFlowComponent>()) return Flow;
		}
	}
	return Actor->FindComponentByClass<UNightFlowComponent>();
}

APlayerController* UNightFlowComponent::GetPlayerController() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner())) return PC;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetController<APlayerController>() : nullptr;
}

void UNightFlowComponent::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->IsLocalController()) return;
	// Ignore an older manually-added Pawn/Blueprint component when a controller instance exists.
	if (const UNightFlowComponent* Existing = PC->FindComponentByClass<UNightFlowComponent>(); Existing && Existing != this) return;
	if (!SettlementWidgetClass) SettlementWidgetClass = USettlementWidget::StaticClass();
	Phase = GetWorld()->GetSubsystem<UHotelPhaseSubsystem>();
	Settlement = GetWorld()->GetSubsystem<UDailySettlementSubsystem>();
	SetComponentTickEnabled(true);
	if (Phase)
	{
		RequestSleepSettlement.BindUObject(Phase.Get(), &UHotelPhaseSubsystem::RequestSleepSettlement);
		Phase->OnPhaseChanged.AddDynamic(this, &ThisClass::HandlePhaseChanged);
		Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
		Phase->OnRunCompleted.AddDynamic(this, &ThisClass::HandleRunCompleted);
		if (Phase->GetCurrentPhase() == EHotelPhase::Settlement) ShowSettlement();
	}
}

void UNightFlowComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 잠자기 안내는 공통 상호작용 UI에서 표시합니다.
	// 검은 배경의 침대 전용 안내는 표시하지 않습니다.
	SetSleepPromptVisible(false);
}

void UNightFlowComponent::SetSleepPromptVisible(bool bVisible)
{
	if (bVisible && !SleepPromptWidget)
	{
		SleepPromptWidget = CreateWidget<UNightSleepPromptWidget>(GetPlayerController());
		if (SleepPromptWidget) SleepPromptWidget->AddToPlayerScreen(100);
	}
	if (SleepPromptWidget)
	{
		SleepPromptWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UNightFlowComponent::CanShowSleepPrompt() const
{
	const APlayerController* PC = GetPlayerController();
	return PC && PC->IsLocalController() && Phase
		&& !bRunCompleted && !bBlackout && !bInputLocked
		&& Phase->CanSleep();
}

bool UNightFlowComponent::CanSleep() const
{
	return CanShowSleepPrompt() && Settlement
		&& SettlementWidgetClass && RequestSleepSettlement.IsBound();
}

bool UNightFlowComponent::RequestSleep()
{
	if (!CanSleep()) return false;
	BlackoutWidget = CreateWidget<UNightBlackoutWidget>(GetPlayerController());
	if (!BlackoutWidget) return false;
	bBlackout = true;
	LockInput();
	BlackoutWidget->AddToPlayerScreen(10000);
	// Only input is blocked. Game time continues throughout the blackout.
	GetWorld()->GetTimerManager().SetTimer(BlackoutTimer, this, &ThisClass::FinishBlackout,
		FMath::Max(0.01f, BlackoutSeconds), false);
	return true;
}

void UNightFlowComponent::FinishBlackout()
{
	bBlackout = false;
	if (Phase && Phase->GetCurrentPhase() != EHotelPhase::Settlement && RequestSleepSettlement.IsBound())
	{
		Settlement->SetPendingEndReason(EDayEndReason::Sleep);
		RequestSleepSettlement.Execute();
	}
	if (Phase && Phase->GetCurrentPhase() == EHotelPhase::Settlement)
	{
		// Only a successful bed sleep completes this objective; automatic settlement
		// reaches HandlePhaseChanged instead and must not count as interaction.
		if (UTutorialSubsystem* Tutorial = GetWorld()->GetSubsystem<UTutorialSubsystem>())
		{
			Tutorial->CompleteStep(ETutorialStep::Sleep);
		}
		ShowSettlement();
	}
	else
	{
		Settlement->SetPendingEndReason(EDayEndReason::Automatic);
		UE_LOG(LogNightFlow, Warning, TEXT("Sleep entry did not enter Settlement. Check RequestSleepSettlement binding."));
		ClearPresentation();
		UnlockInput();
	}
}

void UNightFlowComponent::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase != EHotelPhase::Settlement) return;
	// Capture immediately at 05:00, but finish the existing 0.5 s blackout first.
	if (Settlement) Settlement->FinalizeDay();
	if (!bBlackout) ShowSettlement();
}

void UNightFlowComponent::ShowSettlement()
{
	APlayerController* PC = GetPlayerController();
	if (!PC || !PC->IsLocalController() || bRunCompleted) return;
	if (SettlementWidget) return;
	if (!SettlementWidgetClass || !Settlement || !Settlement->FinalizeDay())
	{
		UE_LOG(LogNightFlow, Warning, TEXT("Cannot show settlement: assign SettlementWidgetClass and check the phase."));
		ClearPresentation();
		UnlockInput();
		return;
	}
	SettlementWidget = CreateWidget<USettlementWidget>(PC, SettlementWidgetClass);
	if (!SettlementWidget)
	{
		ClearPresentation();
		UnlockInput();
		return;
	}
	LockInput();
	SettlementWidget->SetResult(Settlement->GetLastResult(), Phase->GetCurrentDay() >= UHotelPhaseSettings::Get()->MaxDays);
	SettlementWidget->OnConfirmed.AddUObject(this, &ThisClass::ConfirmSettlement);
	SettlementWidget->AddToPlayerScreen(10001);
	if (BlackoutWidget) BlackoutWidget->RemoveFromParent();
	BlackoutWidget = nullptr;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(SettlementWidget->TakeWidget());
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void UNightFlowComponent::ConfirmSettlement()
{
	if (Phase && !bBlackout && !bRunCompleted && Phase->GetCurrentPhase() == EHotelPhase::Settlement)
	{
		Phase->ConfirmSettlement();
	}
}

void UNightFlowComponent::LockInput()
{
	APlayerController* PC = GetPlayerController();
	if (bInputLocked || !PC) return;
	SetSleepPromptVisible(false);
	bInputLocked = true;
	LockedController = PC;
	bPreviousCursor = PC->bShowMouseCursor;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	if (APawn* Pawn = PC->GetPawn())
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately();
		PausedInteraction = Pawn->FindComponentByClass<UPlayerInteractionComponent>();
		if (PausedInteraction.IsValid())
		{
			bInteractionWasTicking = PausedInteraction->IsComponentTickEnabled();
			PausedInteraction->SetComponentTickEnabled(false);
		}
	}
	InputBlocker = NewObject<UInputComponent>(GetOwner(), TEXT("NightInputBlocker"));
	InputBlocker->Priority = MAX_int32;
	InputBlocker->bBlockInput = true;
	InputBlocker->RegisterComponent();
	PC->PushInputComponent(InputBlocker);
	PC->SetInputMode(FInputModeUIOnly());
	PC->bShowMouseCursor = false;
}

void UNightFlowComponent::UnlockInput()
{
	if (!bInputLocked) return;
	if (APlayerController* PC = LockedController.Get())
	{
		if (InputBlocker) PC->PopInputComponent(InputBlocker);
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = bPreviousCursor;
	}
	if (InputBlocker) InputBlocker->DestroyComponent();
	InputBlocker = nullptr;
	if (PausedInteraction.IsValid()) PausedInteraction->SetComponentTickEnabled(bInteractionWasTicking);
	PausedInteraction.Reset();
	LockedController.Reset();
	bInputLocked = false;
}

void UNightFlowComponent::ClearPresentation()
{
	SetSleepPromptVisible(false);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BlackoutTimer);
	bBlackout = false;
	if (BlackoutWidget) BlackoutWidget->RemoveFromParent();
	BlackoutWidget = nullptr;
	if (SettlementWidget)
	{
		SettlementWidget->OnConfirmed.RemoveAll(this);
		SettlementWidget->RemoveFromParent();
	}
	SettlementWidget = nullptr;
}

void UNightFlowComponent::HandleDayStarted(int32 Day)
{
	bRunCompleted = false;
	ClearPresentation();
	UnlockInput();
}

void UNightFlowComponent::HandleRunCompleted(int32 Day)
{
	bRunCompleted = true;
	// Leave the last result visible with its button disabled until the ending UI takes over.
	OnRunCompleted.Broadcast(Day);
}

void UNightFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);
	if (SleepPromptWidget) SleepPromptWidget->RemoveFromParent();
	SleepPromptWidget = nullptr;
	if (Phase)
	{
		Phase->OnPhaseChanged.RemoveAll(this);
		Phase->OnDayStarted.RemoveAll(this);
		Phase->OnRunCompleted.RemoveAll(this);
	}
	ClearPresentation();
	UnlockInput();
	RequestSleepSettlement.Unbind();
	Super::EndPlay(EndPlayReason);
}
