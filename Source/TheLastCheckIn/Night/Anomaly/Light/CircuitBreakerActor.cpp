// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/CircuitBreakerActor.h"
#include "TheLastCheckIn.h"

#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Night/Anomaly/Light/LightInteractionCameraComponent.h"

ACircuitBreakerActor::ACircuitBreakerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BreakerMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("BreakerMesh"));

	SetRootComponent(BreakerMesh);
	
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(
	TEXT("ViewCamera"));

	ViewCamera->SetupAttachment(BreakerMesh);
	ViewCamera->bUsePawnControlRotation = false;
	ViewCamera->SetFieldOfView(60.0f);
	ViewCamera->SetRelativeLocation(FVector(-120.0f, 0.0f, 0.0f));
	CameraSequence = CreateDefaultSubobject<ULightInteractionCameraComponent>(TEXT("CameraSequence"));
	
	BreakerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(
		TEXT("InteractionBox"));

	InteractionBox->SetupAttachment(BreakerMesh);
	InteractionBox->SetBoxExtent(FVector(15.0f, 30.0f, 40.0f));

	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(
		ECC_Visibility, ECR_Block);

	InteractionBox->SetGenerateOverlapEvents(false);
}

void ACircuitBreakerActor::BeginPlay()
{
	Super::BeginPlay();
	LeverAnimation.Initialize(*this, FName(TEXT("PowerPanel_switch")));
	for (UStaticMeshComponent* Lever : LeverAnimation.GetMeshes()) LeverComponentNames.Add(Lever->GetFName());

	if (!IsValid(Anomaly))
	{
		Anomaly = Cast<ALightAnomaly>(
			UGameplayStatics::GetActorOfClass(
				this, ALightAnomaly::StaticClass()));
	}

	if (!IsValid(Anomaly))
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[CircuitBreaker] %s: LightAnomaly를 찾을 수 없습니다."),
			*GetName());
	}
	else
	{
		Anomaly->OnLightingChanged.AddUniqueDynamic(this, &ACircuitBreakerActor::HandleLightingChanged);
		HandleLightingChanged(Anomaly->LightingMode, Anomaly->ActiveZoneId);
	}
}

void ACircuitBreakerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Anomaly))
	{
		Anomaly->OnLightingChanged.RemoveDynamic(this, &ACircuitBreakerActor::HandleLightingChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ACircuitBreakerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LeverAnimation.Tick(*this, DeltaSeconds);
}

void ACircuitBreakerActor::HandleLightingChanged(EHotelLightMode Mode, FName AffectedZoneId)
{
	if (!bCompletingInteraction) ResetLeverPuzzle();
}

bool ACircuitBreakerActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return IsValid(Anomaly)
		&& CameraSequence && CameraSequence->CanStart(Interactor)
		&& Anomaly->GetState() == EAnomalyState::Escalated
		&& Anomaly->LightingMode == EHotelLightMode::Blackout;
}

FText ACircuitBreakerActor::GetInteractPrompt_Implementation(
	AActor* Interactor) const
{
	return NSLOCTEXT(
		"CircuitBreaker", "InteractPrompt", "누전차단기 조작");
}

void ACircuitBreakerActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	UCameraComponent* Camera = IsValid(InteractionCamera) ? InteractionCamera->GetCameraComponent() : ViewCamera.Get();
	CameraSequence->LeverViewTime = 1.0f;
	CameraSequence->StartLeverInteraction(Interactor, Camera, LeverAnimation.GetMeshes(),
		FLightLeverClick::CreateUObject(this, &ThisClass::HandleLeverClick));
}

void ACircuitBreakerActor::ResetLeverPuzzle()
{
	CurrentStep = 0;
	LeverAnimation.SetDown(*this, false, LeverDownRotation);
}

bool ACircuitBreakerActor::HandleLeverClick(UStaticMeshComponent* Lever)
{
	if (!IsValid(Anomaly) || Anomaly->GetState() != EAnomalyState::Escalated
		|| Anomaly->LightingMode != EHotelLightMode::Blackout) return false;
	const int32 Number = LeverComponentNames.IndexOfByKey(Lever->GetFName()) + 1;
	if (Number == 0) return false;
	if (CorrectLeverOrder.IsEmpty())
	{
		const bool bDown = !LeverAnimation.IsDown(Lever);
		LeverAnimation.SetLeverDown(*this, Lever, bDown, LeverDownRotation);
		OnLeverOperated(Number, bDown, true);
		return false;
	}
	// Reject malformed sequences rather than accidentally resolving an incomplete panel.
	TSet<int32> Unique;
	for (int32 Entry : CorrectLeverOrder)
	{
		if (Entry < 1 || Entry > LeverComponentNames.Num() || Unique.Contains(Entry)) return false;
		Unique.Add(Entry);
	}
	if (Unique.Num() != LeverComponentNames.Num() || !CorrectLeverOrder.IsValidIndex(CurrentStep)) return false;
	if (CorrectLeverOrder[CurrentStep] != Number)
	{
		ResetLeverPuzzle();
		OnLeverOperated(Number, false, false);
		return false;
	}
	LeverAnimation.SetLeverDown(*this, Lever, true, LeverDownRotation);
	++CurrentStep;
	OnLeverOperated(Number, true, true);
	if (CurrentStep != CorrectLeverOrder.Num() || IsActorBeingDestroyed()) return false;
	bCompletingInteraction = true;
	const bool bSolved = IsValid(Anomaly) && Anomaly->TryRestoreFromBreaker();
	bCompletingInteraction = false;
	if (bSolved) OnBreakerUsed();
	return bSolved;
}
