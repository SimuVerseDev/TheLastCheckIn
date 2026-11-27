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

    // 기존 슬롯 이름은 BP와 손님 접시에서 계속 사용한다.
    constexpr int32 SlotCount = 12;

    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UFoodSnapSlotComponent* Slot = Index == 0
            ? FoodSlot.Get()
            : CreateDefaultSubobject<UFoodSnapSlotComponent>(
                FName(*FString::Printf(TEXT("FoodSlot_%02d"), Index + 1)));

        Slot->SetupAttachment(Mesh);
        FoodSlots.Add(Slot);
    }
    UpdateSlotLayout();
}

void AFoodPlateActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateSlotLayout();
}

void AFoodPlateActor::BeginPlay()
{
    Super::BeginPlay();
    // 기존 레벨에 저장된 접시에도 새 배치를 적용한다.
    UpdateSlotLayout();
}

void AFoodPlateActor::UpdateSlotLayout()
{
    constexpr int32 Columns = 4;
    for (int32 Index = 0; Index < FoodSlots.Num(); ++Index)
    {
        UFoodSnapSlotComponent* Slot = FoodSlots[Index].Get();
        if (!IsValid(Slot))
        {
            continue;
        }

        // 손님에게 주는 한 끼는 접시 중앙에 놓는다.
        const FVector Location = bGuestOwned && Index == 0
            ? FVector(0.0, 0.0, FoodSlotHeight)
            : FVector(
                (Index % Columns - 1.5f) * FoodSlotSpacing.X,
                (Index / Columns - 1.0f) * FoodSlotSpacing.Y,
                FoodSlotHeight);
        Slot->SetRelativeLocation(Location);
    }
}

AFoodItemActor* AFoodPlateActor::GetFood() const
{
    for (const TObjectPtr<UFoodSnapSlotComponent>& Slot : FoodSlots)
    {
        if (IsValid(Slot.Get()))
        {
            if (AFoodItemActor* Food = Slot->GetFood())
            {
                return Food;
            }
        }
    }
    return nullptr;
}

int32 AFoodPlateActor::GetFoodCount() const
{
    int32 Count = 0;
    for (const TObjectPtr<UFoodSnapSlotComponent>& Slot : FoodSlots)
    {
        if (IsValid(Slot.Get()) && Slot->GetFood())
        {
            ++Count;
        }
    }
    return Count;
}

UFoodSnapSlotComponent* AFoodPlateActor::FindEmptySlot(const AFoodItemActor* Food) const
{
    for (const TObjectPtr<UFoodSnapSlotComponent>& Slot : FoodSlots)
    {
        if (IsValid(Slot.Get()) && Slot->CanAccept(Food))
        {
            return Slot.Get();
        }
    }
    return nullptr;
}

bool AFoodPlateActor::CanInteractWhileCarrying_Implementation(AActor* Interactor) const
{
    return CanInteract_Implementation(Interactor);
}

bool AFoodPlateActor::CanInteract_Implementation(AActor* Interactor) const
{
    return !bGuestOwned
        && !IsCarried()
        && FindEmptySlot(FoodPlacement::GetHeldFood(Interactor)) != nullptr;
}

FText AFoodPlateActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
    return NSLOCTEXT("Food", "PlaceOnPlate", "음식 담기");
}

void AFoodPlateActor::Interact_Implementation(AActor* Interactor)
{
    if (CanInteract_Implementation(Interactor))
    {
        FoodPlacement::TryPlaceHeldFood(
            Interactor, FindEmptySlot(FoodPlacement::GetHeldFood(Interactor)));
    }
}

void AFoodPlateActor::SetGuestOwned()
{
    bGuestOwned = true;
    bCanBePickedUp = false;

    for (const TObjectPtr<UFoodSnapSlotComponent>& Slot : FoodSlots)
    {
        if (IsValid(Slot.Get()))
        {
            Slot->bAllowPickup = false;
        }
    }

    UpdateSlotLayout();

    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFoodPlateActor::ClearFood()
{
    UFoodPoolSubsystem* Pool =
        GetWorld()->GetSubsystem<UFoodPoolSubsystem>();

    for (const TObjectPtr<UFoodSnapSlotComponent>& Slot : FoodSlots)
    {
        AFoodItemActor* Food = IsValid(Slot.Get()) ? Slot->GetFood() : nullptr;
        if (!IsValid(Food))
        {
            continue;
        }

        if (!Pool || !Pool->ReleaseFood(Food, Food->GetLeaseId()))
        {
            // 레벨에 직접 배치한 음식은 풀 소유가 아니므로 제거한다.
            Slot->ForgetFood(Food);
            Food->Destroy();
        }
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
