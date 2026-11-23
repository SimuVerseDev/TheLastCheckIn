// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/CircuitBreakerActor.h"
#include "TheLastCheckIn.h"

#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ACircuitBreakerActor::ACircuitBreakerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BreakerMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("BreakerMesh"));

	SetRootComponent(BreakerMesh);
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
	const bool bDown = Mode == EHotelLightMode::Blackout;
	LeverAnimation.SetDown(*this, bDown, LeverDownRotation);
}

bool ACircuitBreakerActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return IsValid(Anomaly)
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

	if (Anomaly->TryRestoreFromBreaker())
	{
		OnBreakerUsed();
	}
}
