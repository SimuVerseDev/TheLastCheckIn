#include "Combat/CrowbarActor.h"

#include "Combat/CrowbarCombatComponent.h"
#include "Components/StaticMeshComponent.h"

ACrowbarActor::ACrowbarActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);

    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Mesh->SetSimulatePhysics(false);
}

UCrowbarCombatComponent* ACrowbarActor::FindCombat(
    AActor* Actor) const
{
    return IsValid(Actor)
        ? Actor->FindComponentByClass<UCrowbarCombatComponent>()
        : nullptr;
}

bool ACrowbarActor::CanInteract_Implementation(
    AActor* Interactor) const
{
    UCrowbarCombatComponent* Combat = FindCombat(Interactor);

    if (!IsValid(Combat))
    {
        return false;
    }

    if (Combat->GetEquippedCrowbar() == this)
    {
        return true;
    }

    return GetOwner() == nullptr && Combat->CanEquip();
}

FText ACrowbarActor::GetInteractPrompt_Implementation(
    AActor* Interactor) const
{
    const UCrowbarCombatComponent* Combat = FindCombat(Interactor);

    if (IsValid(Combat) && Combat->GetEquippedCrowbar() == this)
    {
        return NSLOCTEXT("Crowbar", "Drop", "빠루 내려놓기");
    }

    return NSLOCTEXT("Crowbar", "Pickup", "빠루 집기");
}

void ACrowbarActor::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return;
    }

    UCrowbarCombatComponent* Combat = FindCombat(Interactor);

    if (Combat->GetEquippedCrowbar() == this)
    {
        Combat->DropCrowbar();
    }
    else
    {
        Combat->TryEquip(this);
    }
}

bool ACrowbarActor::IsInteractionActive_Implementation(AActor* Interactor) const
{
    const UCrowbarCombatComponent* Combat = FindCombat(Interactor);

    return IsValid(Combat) &&
        Combat->GetEquippedCrowbar() == this;
}

void ACrowbarActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UCrowbarCombatComponent* Combat = FindCombat(GetOwner()))
    {
        Combat->NotifyCrowbarRemoved(this);
    }

    Super::EndPlay(EndPlayReason);
}