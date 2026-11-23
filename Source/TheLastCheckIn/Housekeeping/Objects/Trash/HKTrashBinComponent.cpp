// Fill out your copyright notice in the Description page of Project Settings.


#include "Housekeeping/Objects/Trash/HKTrashBinComponent.h"

UHKTrashBinComponent::UHKTrashBinComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHKTrashBinComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxCapacity = FMath::Max(1, MaxCapacity);
	CurrentAmount = FMath::Clamp(CurrentAmount, 0, MaxCapacity);

	NotifyFillChanged();
}

bool UHKTrashBinComponent::CanAcceptTrash(int32 Amount) const
{
	if (Amount <= 0)
	{
		return false;
	}

	const int32 RemainingCapacity =
		GetMaxCapacity() - CurrentAmount;

	return Amount <= RemainingCapacity;
}

bool UHKTrashBinComponent::TryAddTrash(int32 Amount)
{
	if (!CanAcceptTrash(Amount))
	{
		return false;
	}

	CurrentAmount += Amount;
	NotifyFillChanged();

	return true;
}

int32 UHKTrashBinComponent::Empty()
{
	if (CurrentAmount == 0)
	{
		return 0;
	}

	const int32 RemovedAmount = CurrentAmount;

	CurrentAmount = 0;
	NotifyFillChanged();

	return RemovedAmount;
}

float UHKTrashBinComponent::GetFillRatio() const
{
	return FMath::Clamp(
		static_cast<float>(CurrentAmount)
			/ static_cast<float>(GetMaxCapacity()),
		0.0f,
		1.0f);
}

void UHKTrashBinComponent::NotifyFillChanged()
{
	OnFillChanged.Broadcast(
		CurrentAmount,
		GetMaxCapacity(),
		GetFillRatio());
}
