#include "FoodService/Supply/FoodSupplyStation.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Interaction/PlayerInteractionComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FoodService/Data/FoodDefinition.h"
#include "FoodService/Items/FoodItemActor.h"
#include "FoodService/Pool/FoodPoolSubsystem.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"
#include "UObject/ConstructorHelpers.h"

AFoodSupplyStation::AFoodSupplyStation()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
    StationMesh->SetupAttachment(SceneRoot);
    StationMesh->SetCollisionProfileName(TEXT("BlockAll"));
    StationMesh->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ContainerMesh(
        TEXT("/Game/Design/Assets/Restaurant/kitchen_utensils/StaticMeshes/kitchen_utensils_006.kitchen_utensils_006"));
    if (ContainerMesh.Succeeded())
    {
        StationMesh->SetStaticMesh(ContainerMesh.Object);
    }

    // 용기 안쪽을 바라봐도 공급대가 선택되도록 한다.
    InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
    InteractionBounds->SetupAttachment(SceneRoot);
    InteractionBounds->SetBoxExtent(FVector(13.0, 12.0, 5.0));
    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionBounds->SetGenerateOverlapEvents(false);
    InteractionBounds->SetCanEverAffectNavigation(false);

    DisplayFood = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DisplayFood"));
    DisplayFood->SetupAttachment(SceneRoot);
    DisplayFood->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayFood->SetGenerateOverlapEvents(false);
    DisplayFood->SetCanEverAffectNavigation(false);

    PickupPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PickupPoint"));
    PickupPoint->SetupAttachment(SceneRoot);
    PickupPoint->SetRelativeLocation(FVector(0.0, 0.0, 20.0));

    DisplayTransforms.Add(FTransform(FRotator(0.0, -10.0, 0.0), FVector(-3.0, 0.0, -2.0)));
    DisplayTransforms.Add(FTransform(FRotator(0.0, 10.0, 0.0), FVector(3.0, 0.0, 0.0)));
}

void AFoodSupplyStation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    DisplayFood->ClearInstances();
    DisplayFood->SetStaticMesh(IsValid(FoodDefinition) ? FoodDefinition->RawMesh.Get() : nullptr);
    if (!IsValid(FoodDefinition) || !IsValid(FoodDefinition->RawMesh))
    {
        return;
    }

    for (const FTransform& DisplayTransform : DisplayTransforms)
    {
        FTransform InstanceTransform = DisplayTransform;
        InstanceTransform.SetScale3D(DisplayTransform.GetScale3D() * FoodDefinition->MeshScale);
        DisplayFood->AddInstance(InstanceTransform);
    }
}

bool AFoodSupplyStation::CanInteract_Implementation(AActor* Interactor) const
{
    return false;
}

bool AFoodSupplyStation::CanUse_Implementation(AActor* Interactor) const
{
    if (bDispensing || !IsValid(Interactor) || Interactor->GetWorld() != GetWorld()
        || !IsValid(FoodDefinition) || FoodDefinition->FoodId.IsNone()
        || !FoodDefinition->bRequiresCooking || !IsValid(FoodDefinition->RawMesh)
        || !GetWorld() || !GetWorld()->GetSubsystem<UFoodPoolSubsystem>())
    {
        return false;
    }

    const UHKCarryComponent* Carry = Interactor->FindComponentByClass<UHKCarryComponent>();
    UPlayerInteractionComponent* Interaction = Interactor->FindComponentByClass<UPlayerInteractionComponent>();
    return Carry && !Carry->IsCarrying()
        && (!Interaction || !Interaction->HasActiveInteraction());
}

FText AFoodSupplyStation::GetUsePrompt_Implementation(AActor* Interactor) const
{
    return NSLOCTEXT("Food", "TakeIngredient", "재료 꺼내기");
}

bool AFoodSupplyStation::Use_Implementation(AActor* Interactor)
{
    if (!CanUse_Implementation(Interactor))
    {
        return false;
    }

    TGuardValue<bool> DispensingGuard(bDispensing, true);
    UFoodPoolSubsystem* Pool = GetWorld()->GetSubsystem<UFoodPoolSubsystem>();
    AFoodItemActor* Food = Pool->AcquireFood(
        FoodDefinition, EFoodCookState::Raw, PickupPoint->GetComponentTransform());
    if (!IsValid(Food))
    {
        return false;
    }

    const FGuid Lease = Food->GetLeaseId();
    if (IInteractable::Execute_Use(Food, Interactor))
    {
        return true;
    }

    // 집기에 실패한 재료는 공급대 주변에 남기지 않는다.
    Pool->ReleaseFood(Food, Lease);
    return false;
}
