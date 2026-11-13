#include "Guests/Anomaly/Props/ThrowableDecoration.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/StaticMesh.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavFilters/NavigationQueryFilter.h"

AThrowableDecoration::AThrowableDecoration()
{
    PrimaryActorTick.bCanEverTick = false;

    PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
    SetRootComponent(PropMesh);

    PropMesh->SetMobility(EComponentMobility::Movable);
    PropMesh->SetSimulatePhysics(false);
    PropMesh->SetEnableGravity(true);
    PropMesh->SetCollisionObjectType(ECC_PhysicsBody);
    PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PropMesh->SetCollisionResponseToAllChannels(ECR_Block);

    // 작은 장식물이 캐릭터를 밀거나 이동을 막지 않도록 설정
    PropMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    PropMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    PropMesh->SetCanEverAffectNavigation(false);

    ApproachDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("ApproachDirection"));
    ApproachDirection->SetupAttachment(PropMesh);
    ApproachDirection->SetHiddenInGame(true);
    ApproachDirection->SetCanEverAffectNavigation(false);
}

bool AThrowableDecoration::TryBuildActionTransform(
    const APawn* Guest,
    FTransform& OutTransform) const
{
    if (!IsValid(Guest) || !IsValid(ApproachDirection) || IsActorBeingDestroyed()
        || Guest->GetWorld() != GetWorld())
    {
        return false;
    }

    const AAIController* Controller = Cast<AAIController>(Guest->GetController());
    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!IsValid(Controller) || !Navigation)
    {
        return false;
    }

    const FNavAgentProperties& AgentProperties = Guest->GetNavAgentPropertiesRef();
    const FVector StartLocation = Guest->GetNavAgentLocation();
    const ANavigationData* NavData =
        Navigation->GetNavDataForProps(AgentProperties, StartLocation);
    if (!IsValid(NavData))
    {
        return false;
    }

    const FVector ApproachForward = ApproachDirection->GetForwardVector().GetSafeNormal2D();
    if (ApproachForward.IsNearlyZero())
    {
        return false;
    }

    const FSharedConstNavQueryFilter Filter = UNavigationQueryFilter::GetQueryFilter(
        *NavData, Controller, Controller->GetDefaultNavigationFilterClass());
    const FVector PropLocation = GetActorLocation();
    const FVector DesiredLocation =
        PropLocation - ApproachForward * FMath::Max(10.0f, ApproachDistance);
    const FVector ProjectionExtent(
        FMath::Max(1.0, NavigationProjectionExtent.X),
        FMath::Max(1.0, NavigationProjectionExtent.Y),
        FMath::Max(1.0, NavigationProjectionExtent.Z));

    FNavLocation ProjectedLocation;
    if (!Navigation->ProjectPointToNavigation(
            DesiredLocation, ProjectedLocation, ProjectionExtent, NavData, Filter)
        || FVector::DistSquared2D(DesiredLocation, ProjectedLocation.Location)
            > FMath::Square(FMath::Max(0.0f, MaxApproachProjectionError)))
    {
        return false;
    }

    FPathFindingQuery Query(
        Controller, *NavData, StartLocation, ProjectedLocation.Location, Filter);
    Query.SetAllowPartialPaths(false);
    const FPathFindingResult Result = Navigation->FindPathSync(AgentProperties, Query);
    if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial())
    {
        return false;
    }

    const FVector FacingDirection =
        (PropLocation - ProjectedLocation.Location).GetSafeNormal2D();
    if (FacingDirection.IsNearlyZero())
    {
        return false;
    }

    OutTransform = FTransform(FacingDirection.Rotation(), ProjectedLocation.Location);
    return true;
}

bool AThrowableDecoration::CanReserve() const
{
    return !IsActorBeingDestroyed()
        && IsValid(PropMesh)
        && IsValid(PropMesh->GetStaticMesh())
        && !ReservedBy.IsValid()
        && !bHeld;
}

bool AThrowableDecoration::TryReserve(UObject* Requester)
{
    if (!IsValid(Requester) || !CanReserve())
    {
        return false;
    }

    ReservedBy = Requester;
    return true;
}

bool AThrowableDecoration::IsReservedBy(const UObject* Requester) const
{
    return Requester != nullptr && ReservedBy.Get() == Requester;
}

bool AThrowableDecoration::EnablePhysics()
{
    if (!IsValid(PropMesh))
    {
        return false;
    }

    PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PropMesh->SetSimulatePhysics(true);
    return PropMesh->IsSimulatingPhysics();
}

void AThrowableDecoration::ClearPhysicsVelocity()
{
    if (IsValid(PropMesh) && PropMesh->IsSimulatingPhysics())
    {
        PropMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        PropMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
}

bool AThrowableDecoration::KnockDown(UObject* Requester, const FVector& Velocity)
{
    if (!IsReservedBy(Requester) || bHeld)
    {
        return false;
    }

    if (!EnablePhysics())
    {
        return false;
    }

    bPreparedForPickup = false;
    PropMesh->AddImpulse(Velocity, NAME_None, true);
    PropMesh->AddAngularImpulseInDegrees(KnockAngularVelocity, NAME_None, true);
    return true;
}

bool AThrowableDecoration::PreparePickup(
    UObject* Requester,
    const FTransform& PickupTransform)
{
    if (!IsReservedBy(Requester) || !IsValid(PropMesh) || bHeld)
    {
        return false;
    }

    ClearPhysicsVelocity();
    PropMesh->SetSimulatePhysics(false);
    PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 물건 크기는 유지하고 위치와 회전만 변경
    SetActorLocationAndRotation(
        PickupTransform.GetLocation(),
        PickupTransform.Rotator(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    bPreparedForPickup = true;
    return true;
}

bool AThrowableDecoration::Grab(
    UObject* Requester,
    USkeletalMeshComponent* HandMesh)
{
    if (!IsReservedBy(Requester)
        || !IsValid(PropMesh)
        || !IsValid(HandMesh)
        || !HandMesh->DoesSocketExist(GripSocket)
        || bHeld
        || !bPreparedForPickup)
    {
        return false;
    }

    PropMesh->SetSimulatePhysics(false);
    PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (!AttachToComponent(
        HandMesh,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        GripSocket))
    {
        return false;
    }

    SetActorRelativeLocation(GripLocation);
    SetActorRelativeRotation(GripRotation);
    bHeld = true;
    bPreparedForPickup = false;
    return true;
}

bool AThrowableDecoration::Throw(UObject* Requester, const FVector& Velocity)
{
    if (!IsReservedBy(Requester) || !bHeld)
    {
        return false;
    }

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    bHeld = false;
    bPreparedForPickup = false;

    if (!EnablePhysics())
    {
        return false;
    }

    ClearPhysicsVelocity();
    PropMesh->AddImpulse(Velocity, NAME_None, true);
    return true;
}

void AThrowableDecoration::EndInteraction(UObject* Requester)
{
    if (!IsReservedBy(Requester))
    {
        return;
    }

    // 들고 있거나 집기 위치에 고정된 물건만 떨어뜨린다.
    // 이미 날아가는 물건의 위치와 속도는 변경하지 않는다.
    if (bHeld || bPreparedForPickup)
    {
        if (bHeld)
        {
            DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }

        EnablePhysics();
    }

    bHeld = false;
    bPreparedForPickup = false;
    ReservedBy.Reset();
}
