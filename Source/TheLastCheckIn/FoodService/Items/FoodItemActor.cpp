#include "FoodService/Items/FoodItemActor.h"

#include "Components/StaticMeshComponent.h"
#include "FoodService/Placement/FoodSnapSlotComponent.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"

AFoodItemActor::AFoodItemActor()
{
    bRestUntilPickedUp = true;
    bUprightWhileCarried = true;

    PickupPrompt = NSLOCTEXT("Food", "PickupItem", "음식 들기");

    Mesh->SetSimulatePhysics(false);
    Mesh->SetCanEverAffectNavigation(false);
}

void AFoodItemActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshAppearance();
}

void AFoodItemActor::BeginPlay()
{
    Super::BeginPlay();

    if (!bActiveFood && IsValid(Definition))
    {
        const EFoodCookState InitialState =
            CookState == EFoodCookState::Cooking
            ? EFoodCookState::Raw
            : CookState;

        ActivateFood(Definition, InitialState, GetActorTransform());
    }
}

void AFoodItemActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (UFoodSnapSlotComponent* Slot = CurrentSlot.Get())
    {
        Slot->ForgetFood(this);
    }

    CurrentSlot.Reset();

    Super::EndPlay(EndPlayReason);
}

bool AFoodItemActor::IsReadyToEat() const
{
    return bActiveFood
        && IsValid(Definition)
        && (!Definition->bRequiresCooking
            || CookState == EFoodCookState::Cooked);
}

bool AFoodItemActor::MatchesLease(FGuid ExpectedLeaseId) const
{
    return bActiveFood
        && !IsActorBeingDestroyed()
        && ExpectedLeaseId.IsValid()
        && LeaseId == ExpectedLeaseId;
}

UFoodSnapSlotComponent* AFoodItemActor::GetCurrentSlot() const
{
    return CurrentSlot.Get();
}

bool AFoodItemActor::BeginCooking()
{
    if (!bActiveFood
        || !IsValid(Definition)
        || !Definition->bRequiresCooking
        || CookState != EFoodCookState::Raw
        || IsCarried())
    {
        return false;
    }

    CookState = EFoodCookState::Cooking;
    return true;
}

void AFoodItemActor::FinishCooking()
{
    if (CookState != EFoodCookState::Cooking)
    {
        return;
    }

    CookState = EFoodCookState::Cooked;
    RefreshAppearance();
}

void AFoodItemActor::CancelCooking()
{
    if (CookState != EFoodCookState::Cooking)
    {
        return;
    }

    CookState = EFoodCookState::Raw;
    RefreshAppearance();
}

bool AFoodItemActor::CanUse_Implementation(AActor* Interactor) const
{
    const UFoodSnapSlotComponent* Slot = CurrentSlot.Get();

    return bActiveFood
        && CookState != EFoodCookState::Cooking
        && (!Slot || Slot->bAllowPickup)
        && Super::CanUse_Implementation(Interactor);
}

bool AFoodItemActor::Use_Implementation(AActor* Interactor)
{
    if (!CanUse_Implementation(Interactor))
    {
        return false;
    }

    UHKCarryComponent* Carry =
        Interactor->FindComponentByClass<UHKCarryComponent>();

    if (!Carry)
    {
        return false;
    }

    UFoodSnapSlotComponent* PreviousSlot = CurrentSlot.Get();
    const bool bWasSimulating = Mesh->IsSimulatingPhysics();

    // 물리를 켜면 부착이 해제된다. 점유 정보는 집기 성공 후 정리한다.
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetSimulatePhysics(true);

    if (Carry->TryPickup(Mesh))
    {
        if (IsValid(PreviousSlot))
        {
            PreviousSlot->ForgetFood(this);
        }

        return true;
    }

    if (IsValid(PreviousSlot)
        && CurrentSlot.Get() == PreviousSlot)
    {
        Mesh->SetSimulatePhysics(false);

        if (!AttachToComponent(
                PreviousSlot,
                FAttachmentTransformRules::SnapToTargetNotIncludingScale))
        {
            PreviousSlot->ForgetFood(this);
            Mesh->SetSimulatePhysics(true);
            return false;
        }

        SetActorScale3D(Definition->MeshScale);

        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        Mesh->SetCollisionResponseToChannel(
            ECC_Visibility,
            PreviousSlot->bAllowPickup ? ECR_Block : ECR_Ignore);
    }
    else
    {
        Mesh->SetSimulatePhysics(bWasSimulating);
    }

    return false;
}

void AFoodItemActor::RefreshAppearance()
{
    if (!IsValid(Definition))
    {
        return;
    }

    const bool bUseCookedMesh =
        !Definition->bRequiresCooking
        || CookState == EFoodCookState::Cooked;

    Mesh->SetStaticMesh(
        bUseCookedMesh
        ? Definition->CookedMesh.Get()
        : Definition->RawMesh.Get());

    SetActorScale3D(Definition->MeshScale);
}

void AFoodItemActor::ActivateFood(
    UFoodDefinition* InDefinition,
    EFoodCookState InState,
    const FTransform& Transform)
{
    ResetForPool();

    Definition = InDefinition;
    CookState = InState;

    LeaseId = FGuid::NewGuid();
    bActiveFood = true;
    bCanBePickedUp = true;

    SetActorTransform(
        Transform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    RefreshAppearance();

    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetSimulatePhysics(false);

    SetActorEnableCollision(true);
    SetActorHiddenInGame(false);
}

void AFoodItemActor::ResetForPool()
{
    if (UFoodSnapSlotComponent* Slot = CurrentSlot.Get())
    {
        Slot->ForgetFood(this);
    }

    CurrentSlot.Reset();

    Mesh->SetSimulatePhysics(false);

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->EmptyOverrideMaterials();
    Mesh->SetStaticMesh(nullptr);

    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);

    SetOwner(nullptr);
    SetInstigator(nullptr);

    bCanBePickedUp = false;
    bActiveFood = false;

    LeaseId.Invalidate();

    Definition = nullptr;
    CookState = EFoodCookState::Raw;
}