#include "FoodService/Items/FoodPlateActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FoodService/Items/FoodItemActor.h"
#include "FoodService/Placement/FoodPlacement.h"
#include "FoodService/Placement/FoodSnapSlotComponent.h"
#include "FoodService/Pool/FoodPoolSubsystem.h"

AFoodPlateActor::AFoodPlateActor()
{
    bRestUntilPickedUp = true;
    bUprightWhileCarried = true;

    PickupPrompt = NSLOCTEXT("Food", "PickupPlate", "접시 들기");

    Mesh->SetRelativeScale3D(FVector::OneVector);
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCanEverAffectNavigation(false);

    FoodSlot =
        CreateDefaultSubobject<UFoodSnapSlotComponent>(TEXT("FoodSlot"));

    FoodSlot->SetupAttachment(Mesh);
    FoodSlot->SetRelativeLocation(FVector(0.0, 0.0, 3.0));
}

bool AFoodPlateActor::CanInteractWhileCarrying_Implementation(AActor* Interactor) const
{
    return CanInteract_Implementation(Interactor);
}

bool AFoodPlateActor::CanInteract_Implementation(AActor* Interactor) const
{
    return !bGuestOwned
        && !IsCarried()
        && FoodSlot->CanAccept(FoodPlacement::GetHeldFood(Interactor));
}

FText AFoodPlateActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
    return NSLOCTEXT("Food", "PlaceOnPlate", "음식 담기");
}

void AFoodPlateActor::Interact_Implementation(AActor* Interactor)
{
    if (CanInteract_Implementation(Interactor))
    {
        FoodPlacement::TryPlaceHeldFood(Interactor, FoodSlot);
    }
}

void AFoodPlateActor::SetGuestOwned()
{
    bGuestOwned = true;
    bCanBePickedUp = false;

    FoodSlot->bAllowPickup = false;

    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFoodPlateActor::ClearFood()
{
    AFoodItemActor* Food = FoodSlot->GetFood();

    if (!IsValid(Food))
    {
        return;
    }

    const FGuid Lease = Food->GetLeaseId();

    UFoodPoolSubsystem* Pool =
        GetWorld()->GetSubsystem<UFoodPoolSubsystem>();

    if (!Pool || !Pool->ReleaseFood(Food, Lease))
    {
        // 레벨에 직접 배치한 음식은 풀 소유가 아니므로 제거한다.
        FoodSlot->ForgetFood(Food);
        Food->Destroy();
    }
}

void AFoodPlateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bGuestOwned)
    {
        ClearFood();
    }

    Super::EndPlay(EndPlayReason);
}
