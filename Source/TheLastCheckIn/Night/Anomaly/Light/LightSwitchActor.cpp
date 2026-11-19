// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/LightSwitchActor.h"
#include "TheLastCheckIn.h"

#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ALightSwitchActor::ALightSwitchActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("SwitchMesh"));

	SetRootComponent(SwitchMesh);

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
}

bool ALightSwitchActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return IsValid(Anomaly)
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

	if (Anomaly->TryResolveFromSwitch(ZoneId))
	{
		OnSwitchUsed();
	}
}
