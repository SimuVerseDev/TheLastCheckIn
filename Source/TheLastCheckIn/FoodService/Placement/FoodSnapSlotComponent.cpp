#include "FoodService/Placement/FoodSnapSlotComponent.h"

#include "Components/StaticMeshComponent.h"
#include "FoodService/Items/FoodItemActor.h"

AFoodItemActor* UFoodSnapSlotComponent::GetFood() const
{
    AFoodItemActor* Food = Occupant.Get();

    return IsValid(Food)
        && Food->MatchesLease(OccupantLease)
        && Food->GetCurrentSlot() == this
        ? Food
        : nullptr;
}

bool UFoodSnapSlotComponent::CanAccept(const AFoodItemActor* Food) const
{
    if (!IsValid(Food)
        || !Food->IsActiveFood()
        || Food->IsActorBeingDestroyed()
        || !IsValid(Food->GetFoodDefinition())
        || Food->GetWorld() != GetWorld()
        || GetFood()
        || !IsValid(GetOwner())
        || GetOwner()->IsActorBeingDestroyed()
        || GetOwner() == Food
        || Food->GetCookState() == EFoodCookState::Cooking)
    {
        return false;
    }

    if (Rule == EFoodSlotRule::ReadyToEat)
    {
        return Food->IsReadyToEat();
    }

    if (Rule == EFoodSlotRule::RawOnly)
    {
        return Food->GetFoodDefinition()->bRequiresCooking
            && Food->GetCookState() == EFoodCookState::Raw;
    }

    return true;
}

bool UFoodSnapSlotComponent::TryPlaceFood(AFoodItemActor* Food)
{
    if (!CanAccept(Food) || Food->IsCarried())
    {
        return false;
    }

    UFoodSnapSlotComponent* PreviousSlot = Food->GetCurrentSlot();
    UStaticMeshComponent* Mesh = Food->GetMesh();

    const FTransform PreviousTransform = Food->GetActorTransform();
    const bool bWasSimulating = Mesh->IsSimulatingPhysics();

    Mesh->SetSimulatePhysics(false);

    // 새 슬롯 부착에 성공한 뒤 원래 슬롯을 비운다.
    if (!Food->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        Food->SetActorTransform(PreviousTransform);
        Mesh->SetSimulatePhysics(bWasSimulating);
        return false;
    }

    if (PreviousSlot)
    {
        PreviousSlot->ForgetFood(Food);
    }

    Occupant = Food;
    OccupantLease = Food->GetLeaseId();
    Food->CurrentSlot = this;

    Food->SetActorScale3D(Food->GetFoodDefinition()->MeshScale);

    // 붙어 있는 음식은 물리 충돌 없이 집기용 추적만 받는다.
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(
        ECC_Visibility,
        bAllowPickup ? ECR_Block : ECR_Ignore);

    return true;
}

void UFoodSnapSlotComponent::ForgetFood(AFoodItemActor* Food)
{
    if (Occupant.Get() == Food)
    {
        Occupant.Reset();
        OccupantLease.Invalidate();
    }

    if (IsValid(Food) && Food->CurrentSlot.Get() == this)
    {
        Food->CurrentSlot.Reset();
    }
}

void UFoodSnapSlotComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (AFoodItemActor* Food = GetFood())
    {
        ForgetFood(Food);

        Food->CancelCooking();
        Food->DetachFromActor(
            FDetachmentTransformRules::KeepWorldTransform);

        if (EndPlayReason == EEndPlayReason::Destroyed
            || EndPlayReason == EEndPlayReason::RemovedFromWorld)
        {
            Food->GetMesh()->SetCollisionProfileName(TEXT("PhysicsActor"));
            Food->GetMesh()->SetCollisionEnabled(
                ECollisionEnabled::QueryAndPhysics);
            Food->GetMesh()->SetSimulatePhysics(true);
        }
    }

    Super::EndPlay(EndPlayReason);
}