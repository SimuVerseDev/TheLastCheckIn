#include "Housekeeping/Cleaning/HKDirtComponent.h"
#include "GameFramework/Actor.h"

UHKDirtComponent::UHKDirtComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHKDirtComponent::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetOwner()->GetActorTransform();
}

float UHKDirtComponent::GetRawDirt() const
{
	return bDirtActive ? FMath::Clamp(EvaluateDirt(), 0.0f, 1.0f) : 0.0f;
}

float UHKDirtComponent::GetDirt() const
{
	const float Raw = GetRawDirt();
	return Raw <= Tolerance ? 0.0f : Raw;
}

void UHKDirtComponent::ApplyDirt(FRandomStream& Random)
{
	bDirtActive = true;
}

void UHKDirtComponent::ClearDirt()
{
	bDirtActive = false;
}