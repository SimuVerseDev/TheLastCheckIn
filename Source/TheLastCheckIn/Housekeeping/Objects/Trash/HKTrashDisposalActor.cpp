// Fill out your copyright notice in the Description page of Project Settings.


#include "Housekeeping/Objects/Trash/HKTrashDisposalActor.h"

#include "Housekeeping/Objects/Trash/HKTrashBinComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"

AHKTrashDisposalActor::AHKTrashDisposalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DisposalMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("DisposalMesh"));

	SetRootComponent(DisposalMesh);

	// 메인 쓰레기통의 외형은 실제 장애물로 사용합니다.
	DisposalMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DisposalMesh->SetSimulatePhysics(false);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(
		TEXT("InteractionBox"));

	InteractionBox->SetupAttachment(DisposalMesh);

	// 실제 메시 크기에 맞게 BP에서 조절합니다.
	InteractionBox->SetBoxExtent(FVector(50.0f, 50.0f, 70.0f));
	InteractionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));

	InteractionBox->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);

	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);

	InteractionBox->SetCollisionResponseToChannel(
		ECC_Visibility,
		ECR_Block);

	InteractionBox->SetGenerateOverlapEvents(false);

	EmptyPrompt = NSLOCTEXT(
		"Housekeeping",
		"EmptyTrashBinPrompt",
		"쓰레기통 비우기");
}

bool AHKTrashDisposalActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return false;
}

bool AHKTrashDisposalActor::CanUse_Implementation(
	AActor* Interactor) const
{
	const UHKTrashBinComponent* Bin = FindHeldTrashBin(Interactor);

	return IsValid(Bin) && !Bin->IsEmpty();
}

FText AHKTrashDisposalActor::GetUsePrompt_Implementation(
	AActor* Interactor) const
{
	return EmptyPrompt;
}

bool AHKTrashDisposalActor::Use_Implementation(
	AActor* Interactor)
{
	UHKTrashBinComponent* Bin = FindHeldTrashBin(Interactor);

	if (!IsValid(Bin))
	{
		return false;
	}

	const int32 RemovedAmount = Bin->Empty();

	if (RemovedAmount <= 0)
	{
		return false;
	}

	OnTrashEmptied(RemovedAmount);
	return true;
}

UHKTrashBinComponent* AHKTrashDisposalActor::FindHeldTrashBin(
	AActor* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return nullptr;
	}

	const UHKCarryComponent* Carry =
		Interactor->FindComponentByClass<UHKCarryComponent>();

	return IsValid(Carry) ? Carry->GetHeldTrashBin() : nullptr;
}
