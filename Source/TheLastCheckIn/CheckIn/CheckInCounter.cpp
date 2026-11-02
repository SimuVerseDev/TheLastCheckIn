#include "CheckInCounter.h"
#include "CheckInInteractionComponent.h"
#include "Components/ArrowComponent.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Party/GuestParty.h"

bool ACheckInCounter::CanInteract_Implementation(AActor* Interactor) const
{
	const UCheckInInteractionComponent* CheckIn = Interactor ? Interactor->FindComponentByClass<UCheckInInteractionComponent>() : nullptr;
	return CheckIn && CheckIn->CanOpenCheckIn();
}

FText ACheckInCounter::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("CheckIn", "CounterInteractPrompt", "체크인");
}

void ACheckInCounter::Interact_Implementation(AActor* Interactor)
{
	if (UCheckInInteractionComponent* CheckIn = Interactor ? Interactor->FindComponentByClass<UCheckInInteractionComponent>() : nullptr)
	{
		CheckIn->OpenCheckIn();
	}
}


bool ACheckInCounter::GetSlotTransform(
    int32 SlotIndex,
    FTransform& OutTransform) const
{
    static const FName SlotNames[] =
    {
        TEXT("ReceptionSlot"),
        TEXT("CompanionSlot1"),
        TEXT("CompanionSlot2"),
        TEXT("CompanionSlot3")
    };

    if (SlotIndex < 0 || SlotIndex >= UE_ARRAY_COUNT(SlotNames))
    {
        return false;
    }

    TInlineComponentArray<UArrowComponent*> Arrows;
    GetComponents(Arrows);

    for (UArrowComponent* Arrow : Arrows)
    {
        if (IsValid(Arrow) &&
            Arrow->GetFName() == SlotNames[SlotIndex])
        {
            OutTransform = Arrow->GetComponentTransform();
            return true;
        }
    }

    return false;
}

bool ACheckInCounter::HasSlots(int32 RequiredCount) const
{
    if (RequiredCount < 1 || RequiredCount > 4)
    {
        return false;
    }

    for (int32 Index = 0; Index < RequiredCount; ++Index)
    {
        FTransform Slot;

        if (!GetSlotTransform(Index, Slot))
        {
            return false;
        }
    }

    return true;
}

UGuestParty* ACheckInCounter::GetCurrentParty() const
{
    UGuestParty* Party = CurrentParty.Get();

    return IsValid(Party) && Party->IsCheckingIn()
        ? Party
        : nullptr;
}

bool ACheckInCounter::CanAcceptParty() const
{
    UGuestParty* Party = CurrentParty.Get();

    if (!IsValid(Party) || Party->IsFinished())
    {
        return true;
    }

    if (Party->IsCheckingIn())
    {
        return false;
    }

    // 이전 일행이 자리 주변을 벗어날 때까지 다음 접수를 기다림.
    for (AGuestCharacter* Guest : Party->GetMembers())
    {
        if (!IsValid(Guest) || Guest->IsHidden())
        {
            continue;
        }

        const FVector GuestLocation = Guest->GetActorLocation();

        for (int32 Index = 0; Index < 4; ++Index)
        {
            FTransform Slot;

            if (!GetSlotTransform(Index, Slot))
            {
                continue;
            }

            const FVector SlotLocation = Slot.GetLocation();

            if (FMath::Abs(GuestLocation.Z - SlotLocation.Z) < 200.0 &&
                FVector::DistSquared2D(GuestLocation, SlotLocation)
                    < FMath::Square(150.0))
            {
                return false;
            }
        }
    }

    return true;
}

bool ACheckInCounter::RegisterParty(UGuestParty* InParty)
{
    if (!IsValid(InParty) ||
        !InParty->IsCheckingIn() ||
        !HasSlots(InParty->GetSize()))
    {
        return false;
    }

    if (CurrentParty.Get() == InParty)
    {
        return true;
    }

    if (!CanAcceptParty())
    {
        return false;
    }

    CurrentParty = InParty;
    return true;
}

void ACheckInCounter::UnregisterParty(UGuestParty* InParty)
{
    if (CurrentParty.Get() == InParty)
    {
        CurrentParty.Reset();
    }
}

bool ACheckInCounter::GetSlotTransformForGuest(
    const AGuestCharacter* Guest,
    FTransform& OutTransform) const
{
    UGuestParty* Party = GetCurrentParty();

    if (!IsValid(Guest) ||
        !IsValid(Party) ||
        Guest->GetParty() != Party)
    {
        return false;
    }

    const TArray<AGuestCharacter*> Members = Party->GetMembers();

    if (Members.Num() != Party->GetSize())
    {
        return false;
    }

    const int32 Index = Members.IndexOfByPredicate(
        [Guest](const AGuestCharacter* Member)
        {
            return Member == Guest;
        });

    return GetSlotTransform(Index, OutTransform);
}