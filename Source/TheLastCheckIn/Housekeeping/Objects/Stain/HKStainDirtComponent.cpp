#include "Housekeeping/Objects/Stain/HKStainDirtComponent.h"

void UHKStainDirtComponent::ApplyDirt(FRandomStream& Random)
{
	Super::ApplyDirt(Random);
	SetAmount(1.0f);
}

void UHKStainDirtComponent::ClearDirt()
{
	Super::ClearDirt();
	SetAmount(0.0f);
}

void UHKStainDirtComponent::AddWipe(float Distance)
{
	if (!IsDirtActive() || Distance <= 0.0f)
	{
		return;
	}
	const float NewAmount = Amount - Distance / WipeDistanceToClean;
	SetAmount(NewAmount <= Tolerance ? 0.0f : NewAmount); // 임계치 넘으면 얼룩 완전히 없애기
}

void UHKStainDirtComponent::SetAmount(float NewAmount)
{
	NewAmount = FMath::Clamp(NewAmount, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Amount, NewAmount))
	{
		return;
	}
	Amount = NewAmount;
	OnAmountChanged.Broadcast(Amount);
}