#include "FoodService/Placement/FoodPlacement.h"

#include "Components/StaticMeshComponent.h"
#include "FoodService/Items/FoodItemActor.h"
#include "FoodService/Items/FoodPlateActor.h"
#include "FoodService/Placement/FoodSnapSlotComponent.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"

AFoodItemActor* FoodPlacement::GetHeldFood(AActor* Interactor)
{
    UHKCarryComponent* Carry = IsValid(Interactor)
        ? Interactor->FindComponentByClass<UHKCarryComponent>()
        : nullptr;

    UPrimitiveComponent* Held = Carry ? Carry->GetCarriedComponent() : nullptr;

    if (!IsValid(Held))
    {
        return nullptr;
    }

    if (AFoodItemActor* Food =
        Cast<AFoodItemActor>(Held->GetOwner()))
    {
        return Food;
    }

    if (AFoodPlateActor* Plate =
        Cast<AFoodPlateActor>(Held->GetOwner()))
    {
        return Plate->GetFood();
    }

    return nullptr;
}

bool FoodPlacement::TryPlaceHeldFood(AActor* Interactor, UFoodSnapSlotComponent* Slot)
{
    AFoodItemActor* Food = GetHeldFood(Interactor);

    if (!IsValid(Slot) || !Slot->CanAccept(Food))
    {
        return false;
    }

    UHKCarryComponent* Carry =
        Interactor->FindComponentByClass<UHKCarryComponent>();

    UPrimitiveComponent* Held =
        Carry ? Carry->GetCarriedComponent() : nullptr;

    if (!IsValid(Held) || Held->GetOwner() == Slot->GetOwner())
    {
        return false;
    }

    // 접시를 들고 있다면 접시는 그대로 두고 음식만 옮긴다.
    const bool bHoldingFood = Held == Food->GetMesh();

    const FTransform PreviousTransform = Food->GetActorTransform();
    const FGuid Lease = Food->GetLeaseId();

    if (bHoldingFood && !Carry->ReleaseForPlacement(Food->GetMesh()))
    {
        return false;
    }

    if (!IsValid(Food) || !Food->MatchesLease(Lease))
    {
        return false;
    }

    if (Slot->TryPlaceFood(Food))
    {
        return true;
    }

    // 해제 후 배치가 실패하면 다시 집기를 시도한다.
    if (bHoldingFood
        && !Food->IsCarried()
        && !Food->GetCurrentSlot())
    {
        Food->SetActorTransform(PreviousTransform);

        Food->GetMesh()->SetCollisionProfileName(TEXT("PhysicsActor"));
        Food->GetMesh()->SetCollisionEnabled(
            ECollisionEnabled::QueryAndPhysics);
        Food->GetMesh()->SetSimulatePhysics(true);

        Carry->TryPickup(Food->GetMesh());
    }

    return false;
}
