#include "FoodService/Items/FoodItemActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AFoodItemActor::AFoodItemActor()
{
    // 집기 전에는 위치를 고정하고, 집을 때 물리를 켠다.
    bRestUntilPickedUp = true;
    bUprightWhileCarried = true;
    bCanBePickedUp = false;

    PickupPrompt = NSLOCTEXT("Food", "Pickup", "음식 들기");

    Mesh->SetSimulatePhysics(false);
    Mesh->SetStaticMesh(nullptr);
    Mesh->SetRelativeScale3D(FVector::OneVector);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

bool AFoodItemActor::MatchesLease(FGuid ExpectedLeaseId) const
{
    return !IsActorBeingDestroyed()
        && bActiveFood
        && ExpectedLeaseId.IsValid()
        && LeaseId == ExpectedLeaseId;
}

void AFoodItemActor::ActivateFood(
    UFoodDefinition* InDefinition,
    int32 InServings,
    EFoodPresentation InPresentation,
    const FTransform& Transform)
{
    ResetForPool();

    Definition = InDefinition;
    Servings = InServings;
    Presentation = InPresentation;

    // 이전 음식의 요청과 구분하도록 대여 번호를 갱신한다.
    LeaseId = FGuid::NewGuid();
    bActiveFood = true;

    UStaticMesh* FoodMesh =
        Presentation == EFoodPresentation::Batch
        ? Definition->BatchMesh.Get()
        : Definition->ServingMesh.Get();

    Mesh->SetStaticMesh(FoodMesh);

    SetActorTransform(
        Transform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    bCanBePickedUp = true;

    SetActorEnableCollision(true);
    SetActorHiddenInGame(false);
}

void AFoodItemActor::ResetForPool()
{
    // 집기를 막고 대여 번호를 무효화한다.
    bCanBePickedUp = false;
    bActiveFood = false;
    LeaseId.Invalidate();

    // 재사용 후 이전 타이머가 실행되지 않도록 정리한다.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }

    SetLifeSpan(0.0f);

    if (Mesh->IsSimulatingPhysics())
    {
        Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    Mesh->SetSimulatePhysics(false);

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    SetOwner(nullptr);
    SetInstigator(nullptr);

    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    SetActorTickEnabled(false);

    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->EmptyOverrideMaterials();
    Mesh->SetStaticMesh(nullptr);

    Definition = nullptr;
    Servings = 0;
    Presentation = EFoodPresentation::Batch;
}
