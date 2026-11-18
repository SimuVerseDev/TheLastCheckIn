#include "Trash/TrashBagHolderComponent.h"
#include "Trash/TrashBagActor.h"

UTrashBagHolderComponent::UTrashBagHolderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UTrashBagHolderComponent* UTrashBagHolderComponent::FindOrAdd(AActor* Holder)
{
	if (!IsValid(Holder))
	{
		return nullptr;
	}
	if (UTrashBagHolderComponent* Existing = Holder->FindComponentByClass<UTrashBagHolderComponent>())
	{
		return Existing;
	}
	UTrashBagHolderComponent* Added = NewObject<UTrashBagHolderComponent>(Holder, TEXT("TrashBagHolder"));
	Holder->AddInstanceComponent(Added);
	Added->RegisterComponent();
	return Added;
}

void UTrashBagHolderComponent::SetHeldBag(ATrashBagActor* Bag)
{
	HeldBag = Bag;
}

void UTrashBagHolderComponent::ClearHeldBag(const ATrashBagActor* Bag)
{
	if (!HeldBag.IsValid() || HeldBag.Get() == Bag)
	{
		HeldBag.Reset();
	}
}

ATrashBagActor* UTrashBagHolderComponent::GetHeldBag() const
{
	return HeldBag.Get();
}

bool UTrashBagHolderComponent::IsHolding() const
{
	return HeldBag.IsValid();
}

bool UTrashBagHolderComponent::TryHandleUseStarted()
{
	// 봉투를 든 동안 LMB는 소비만 함
	return IsHolding();
}

void UTrashBagHolderComponent::GetIgnoredActors(TArray<AActor*>& IgnoredActors) const
{
	if (ATrashBagActor* Bag = HeldBag.Get())
	{
		IgnoredActors.Add(Bag);
	}
}
