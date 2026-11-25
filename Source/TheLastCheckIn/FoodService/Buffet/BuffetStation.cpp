#include "FoodService/Buffet/BuffetStation.h"

#include "FoodService/Data/FoodDefinition.h"
#include "FoodService/Items/FoodItemActor.h"
#include "FoodService/Pool/FoodPoolSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"

ABuffetStation::ABuffetStation()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("StationMesh"));
    StationMesh->SetupAttachment(SceneRoot);

    // 음식은 메시 하나로 표시하고, 남은 양은 RemainingServings로 관리한다.
    FoodDisplay = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("FoodDisplay"));
    FoodDisplay->SetupAttachment(SceneRoot);
    FoodDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FoodDisplay->SetGenerateOverlapEvents(false);
    FoodDisplay->SetVisibility(false);

    ArrivalArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ArrivalArea"));
    ArrivalArea->SetupAttachment(SceneRoot);
    ArrivalArea->SetBoxExtent(FVector(80.0, 80.0, 120.0));
    ArrivalArea->SetRelativeLocation(FVector(150.0, 0.0, 0.0));
    ArrivalArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArrivalArea->SetGenerateOverlapEvents(false);

    // 손님 EQS가 도착 영역을 찾을 때 사용하는 태그
    ArrivalArea->ComponentTags.Add(TEXT("GuestArrivalArea"));

    ServingDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("ServingDirection"));

    ServingDirection->SetupAttachment(SceneRoot);
    ServingDirection->SetHiddenInGame(true);
    ServingDirection->SetCanEverAffectNavigation(false);
}

int32 ABuffetStation::TryAddFood(
    AFoodItemActor* Food,
    FGuid ExpectedLeaseId)
{
    if (bUpdatingStock)
    {
        return 0;
    }

    TGuardValue<bool> StockGuard(bUpdatingStock, true);

    UFoodPoolSubsystem* Pool =
        GetWorld()->GetSubsystem<UFoodPoolSubsystem>();

    if (!Pool
        || !IsValid(Food)
        || Food->GetWorld() != GetWorld()
        || Food->PoolOwner.Get() != Pool
        || !Food->MatchesLease(ExpectedLeaseId)
        || Food->IsCarried()
        || Food->GetAttachParentActor() != nullptr
        || Food->GetPresentation() != EFoodPresentation::Batch)
    {
        return 0;
    }

    UFoodDefinition* Incoming = Food->GetFoodDefinition();

    if (!IsValid(Incoming)
        || !IsValid(Incoming->ServingMesh.Get()))
    {
        return 0;
    }

    // 음식이 남아 있으면 같은 종류만 보충한다.
    if (RemainingServings > 0 && StockDefinition.Get() != Incoming)
    {
        return 0;
    }

    const int32 Space = FMath::Max(
        0,
        Capacity - RemainingServings);

    const int32 Added = FMath::Min(Space, Food->Servings);

    if (Added <= 0)
    {
        return 0;
    }

    Food->Servings -= Added;

    // 모든 인분을 옮겼으면 운반용 음식 액터를 풀 반환한다.
    if (Food->Servings == 0)
    {
        if (!Pool->ReleaseFood(Food, ExpectedLeaseId))
        {
            // 풀 반환 실패 시 옮기려던 수량을 복구한다.
            Food->Servings += Added;
            return 0;
        }
    }

    StockDefinition = Incoming;
    RemainingServings += Added;

    RefreshFoodDisplay();

    return Added;
}

bool ABuffetStation::HasFood() const
{
    return RemainingServings > 0
        && IsValid(StockDefinition.Get());
}

bool ABuffetStation::TryReserveServingPoint(
    AActor* Requester,
    FGuid RequestId)
{
    RemoveInvalidQueueGuests();

    if (!HasFood()
        || !IsValid(Requester)
        || Requester->IsActorBeingDestroyed()
        || Requester->GetWorld() != GetWorld()
        || !RequestId.IsValid())
    {
        return false;
    }

    if (ReservedGuest.IsValid()
        && !ReservedGuest->IsActorBeingDestroyed())
    {
        return OwnsReservation(Requester, RequestId);
    }

    if (WaitingQueue.IsEmpty()
        || WaitingQueue[0].Guest.Get() != Requester
        || WaitingQueue[0].VisitId != RequestId)
    {
        return false;
    }

    ReservedGuest = Requester;
    ReservationId = RequestId;

    // 음식 앞으로 이동하는 손님을 빼고 뒤 손님들의 순서를 당긴다.
    WaitingQueue.RemoveAt(0);

    return true;
}

void ABuffetStation::RemoveInvalidQueueGuests()
{
    WaitingQueue.RemoveAll([](const FQueueEntry& Entry)
    {
        const AActor* Guest = Entry.Guest.Get();
        return !IsValid(Guest) || Guest->IsActorBeingDestroyed();
    });
}

bool ABuffetStation::TryJoinQueue(AActor* Requester, FGuid RequestId)
{
    RemoveInvalidQueueGuests();

    if (!HasFood()
        || !IsValid(Requester)
        || Requester->IsActorBeingDestroyed()
        || Requester->GetWorld() != GetWorld()
        || !RequestId.IsValid()
        || QueuePoints.IsEmpty())
    {
        return false;
    }

    // 비어 있거나 중복된 대기 위치에는 손님을 배정하지 않는다.
    TSet<ATargetPoint*> UsedPoints;
    for (const TObjectPtr<ATargetPoint>& PointRef : QueuePoints)
    {
        ATargetPoint* Point = PointRef.Get();
        if (!IsValid(Point) || Point->IsActorBeingDestroyed()
            || Point->GetWorld() != GetWorld() || UsedPoints.Contains(Point))
        {
            return false;
        }
        UsedPoints.Add(Point);
    }

    for (const FQueueEntry& Entry : WaitingQueue)
    {
        if (Entry.Guest.Get() == Requester)
        {
            return Entry.VisitId == RequestId;
        }
    }

    if (ReservedGuest.Get() == Requester || WaitingQueue.Num() >= QueuePoints.Num())
    {
        return false;
    }

    WaitingQueue.Add({Requester, RequestId});
    return true;
}

AActor* ABuffetStation::GetQueuePoint(AActor* Requester, FGuid RequestId)
{
    RemoveInvalidQueueGuests();

    const int32 Index = WaitingQueue.IndexOfByPredicate(
        [Requester, RequestId](const FQueueEntry& Entry)
        {
            return Entry.Guest.Get() == Requester && Entry.VisitId == RequestId;
        });

    if (!QueuePoints.IsValidIndex(Index))
    {
        return nullptr;
    }

    ATargetPoint* Point = QueuePoints[Index].Get();
    return IsValid(Point) && !Point->IsActorBeingDestroyed() ? Point : nullptr;
}

void ABuffetStation::LeaveQueue(AActor* Requester, FGuid RequestId)
{
    WaitingQueue.RemoveAll([Requester, RequestId](const FQueueEntry& Entry)
    {
        return Entry.Guest.Get() == Requester && Entry.VisitId == RequestId;
    });

    ReleaseServingPoint(Requester, RequestId);
}

bool ABuffetStation::OwnsReservation(
    AActor* Requester,
    FGuid RequestId) const
{
    return IsValid(Requester)
        && !Requester->IsActorBeingDestroyed()
        && ReservedGuest.Get() == Requester
        && RequestId.IsValid()
        && ReservationId == RequestId;
}

void ABuffetStation::ReleaseServingPoint(
    AActor* Requester,
    FGuid RequestId)
{
    // 손님과 방문 번호가 모두 일치해야 예약을 해제한다.
    if (ReservedGuest.Get() == Requester
        && RequestId.IsValid()
        && ReservationId == RequestId)
    {
        ReservedGuest.Reset();
        ReservationId.Invalidate();
    }
}

AFoodItemActor* ABuffetStation::TryTakeServing(
    AActor* Requester,
    FGuid RequestId,
    const FTransform& PlateTransform)
{
    if (bUpdatingStock
        || !HasFood()
        || !OwnsReservation(Requester, RequestId))
    {
        return nullptr;
    }

    TGuardValue<bool> StockGuard(bUpdatingStock, true);

    UFoodPoolSubsystem* Pool =
        GetWorld()->GetSubsystem<UFoodPoolSubsystem>();

    if (!Pool)
    {
        return nullptr;
    }

    AFoodItemActor* Plate = Pool->AcquireFood(
        StockDefinition.Get(),
        1,
        EFoodPresentation::Serving,
        PlateTransform);

    if (!IsValid(Plate))
    {
        return nullptr;
    }

    // 접시가 준비된 뒤에만 재고에서 1인분을 차감한다.
    --RemainingServings;

    ReleaseServingPoint(Requester, RequestId);
    RefreshFoodDisplay();

    return Plate;
}

FRotator ABuffetStation::GetServingRotation() const
{
    const float Yaw = IsValid(ServingDirection)
    ? ServingDirection->GetComponentRotation().Yaw
    : GetActorRotation().Yaw;

    // 좌우 방향만 적용한다.
    return FRotator(0.0f, Yaw, 0.0f);
}

void ABuffetStation::RefreshFoodDisplay()
{
    FoodDisplay->SetStaticMesh(
        HasFood() ? StockDefinition->BatchMesh.Get() : nullptr);

    FoodDisplay->SetVisibility(HasFood());
}
