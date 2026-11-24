// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/LightSwitchActor.h"
#include "TheLastCheckIn.h"

#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Night/Anomaly/Light/LightInteractionCameraComponent.h"

ALightSwitchActor::ALightSwitchActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("SwitchMesh"));

	SetRootComponent(SwitchMesh);
	
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(
	TEXT("ViewCamera"));

	ViewCamera->SetupAttachment(SwitchMesh);
	ViewCamera->bUsePawnControlRotation = false;
	ViewCamera->SetFieldOfView(60.0f);
	ViewCamera->SetRelativeLocation(FVector(-60.0f, 0.0f, 0.0f));
	CameraSequence = CreateDefaultSubobject<ULightInteractionCameraComponent>(TEXT("CameraSequence"));

	// 상호작용 판정은 아래의 박스가 담당합니다.
	SwitchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(
		TEXT("InteractionBox"));

	InteractionBox->SetupAttachment(SwitchMesh);
	InteractionBox->SetBoxExtent(FVector(12.0f, 12.0f, 18.0f));

	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(
		ECC_Visibility, ECR_Block);

	InteractionBox->SetGenerateOverlapEvents(false);
}

void ALightSwitchActor::BeginPlay()
{
	Super::BeginPlay();
	LeverAnimation.Initialize(*this, FName(TEXT("SM_switch_button")));

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
			TEXT("[LightSwitch] %s: LightAnomaly를 찾을 수 없습니다."),
			*GetName());
	}
	else
	{
		Anomaly->OnLightingChanged.AddUniqueDynamic(this, &ALightSwitchActor::HandleLightingChanged);
		HandleLightingChanged(Anomaly->LightingMode, Anomaly->ActiveZoneId);
	}
}

void ALightSwitchActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Anomaly))
	{
		Anomaly->OnLightingChanged.RemoveDynamic(this, &ALightSwitchActor::HandleLightingChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ALightSwitchActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LeverAnimation.Tick(*this, DeltaSeconds);
}

void ALightSwitchActor::HandleLightingChanged(EHotelLightMode Mode, FName AffectedZoneId)
{
	const bool bDown = Mode == EHotelLightMode::Flickering && !ZoneId.IsNone() && ZoneId == AffectedZoneId;
	LeverAnimation.SetDown(*this, bDown, LeverDownRotation);
}

bool ALightSwitchActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return IsValid(Anomaly)
		&& CameraSequence && CameraSequence->CanStart(Interactor)
		&& !ZoneId.IsNone()
		&& Anomaly->GetState() == EAnomalyState::Active
		&& Anomaly->LightingMode == EHotelLightMode::Flickering
		&& Anomaly->ActiveZoneId == ZoneId;
}

FText ALightSwitchActor::GetInteractPrompt_Implementation(
	AActor* Interactor) const
{
	return NSLOCTEXT(
		"LightSwitch", "InteractPrompt", "조명 스위치 조작");
}

void ALightSwitchActor::Interact_Implementation(
	AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	CameraSequence->LeverViewTime = 1.0f;
	CameraSequence->StartLeverInteraction(Interactor, ViewCamera, LeverAnimation.GetMeshes(),
		FLightLeverClick::CreateWeakLambda(this, [this](UStaticMeshComponent* Lever)
	{
		if (IsValid(Anomaly) && Anomaly->TryResolveFromSwitch(ZoneId))
		{
			LeverAnimation.SetDown(*this, false, LeverDownRotation);
			OnSwitchUsed();
			return true;
		}
		return false;
	}));
}
