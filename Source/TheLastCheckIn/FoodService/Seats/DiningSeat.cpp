#include "FoodService/Seats/DiningSeat.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TargetPoint.h"

ADiningSeat::ADiningSeat()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
    ChairMesh->SetupAttachment(SceneRoot);
    ChairMesh->SetCollisionProfileName(TEXT("BlockAll"));
    ChairMesh->SetCanEverAffectNavigation(true);

    SeatPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SeatPoint"));
    SeatPoint->SetupAttachment(SceneRoot);

    // 실제 손님 캡슐 높이에 맞춰 BP에서 조정한다.
    SeatPoint->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
}

bool ADiningSeat::TryReserve(AActor* Guest, const FGuid& ReservationId)
{
    if (IsActorBeingDestroyed()
        || !IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetWorld() != GetWorld()
        || !ReservationId.IsValid()
        || !IsValid(ApproachPoint.Get())
        || ApproachPoint->IsActorBeingDestroyed()
        || ApproachPoint->GetWorld() != GetWorld())
    {
        return false;
    }

    AActor* CurrentGuest = ReservedGuest.Get();

    if (IsValid(CurrentGuest)
        && !CurrentGuest->IsActorBeingDestroyed())
    {
        return IsReservedBy(Guest, ReservationId);
    }

    ReservedGuest = Guest;
    ActiveReservationId = ReservationId;
    return true;
}

bool ADiningSeat::IsReservedBy(const AActor* Guest, const FGuid& ReservationId) const
{
    return IsValid(Guest)
        && ReservationId.IsValid()
        && ReservedGuest.Get() == Guest
        && ActiveReservationId == ReservationId;
}

void ADiningSeat::Release(const AActor* Guest, const FGuid& ReservationId)
{
    if (!IsReservedBy(Guest, ReservationId))
    {
        return;
    }

    ReservedGuest.Reset();
    ActiveReservationId.Invalidate();
}

ATargetPoint* ADiningSeat::GetApproachPoint() const
{
    return ApproachPoint.Get();
}

FTransform ADiningSeat::GetSeatTransform() const
{
    return SeatPoint->GetComponentTransform();
}