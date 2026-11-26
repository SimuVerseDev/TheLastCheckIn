#include "FoodService/Buffet/BuffetStation.h"

#include "FoodService/Data/FoodDefinition.h"
#include "FoodService/Items/FoodItemActor.h"
#include "FoodService/Items/FoodPlateActor.h"
#include "FoodService/Placement/FoodPlacement.h"
#include "FoodService/Placement/FoodSnapSlotComponent.h"
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

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
    StationMesh->SetupAttachment(SceneRoot);
    StationMesh->SetCollisionProfileName(TEXT("BlockAll"));
    StationMesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);

    ArrivalArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ArrivalArea"));
    ArrivalArea->SetupAttachment(SceneRoot);
    ArrivalArea->SetBoxExtent(FVector(80.0, 80.0, 120.0));
    ArrivalArea->SetRelativeLocation(FVector(150.0, 0.0, 0.0));
    ArrivalArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArrivalArea->SetGenerateOverlapEvents(false);
    ArrivalArea->ComponentTags.Add(TEXT("GuestArrivalArea"));

    ServingDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("ServingDirection"));
    ServingDirection->SetupAttachment(SceneRoot);
    ServingDirection->SetHiddenInGame(true);
    ServingDirection->SetCanEverAffectNavigation(false);

    constexpr int32 SlotCount = 12;
    constexpr int32 Columns = 4;
    constexpr float Spacing = 12.0f;

    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const FName SlotName(*FString::Printf(TEXT("FoodSlot_%02d"), Index + 1));

        UFoodSnapSlotComponent* Slot =
            CreateDefaultSubobject<UFoodSnapSlotComponent>(SlotName);

        Slot->SetupAttachment(SceneRoot);
        Slot->Rule = EFoodSlotRule::ReadyToEat;

        // 진열된 음식이 트레이의 E키 판정을 가리지 않게 한다.
        Slot->bAllowPickup = false;

        const int32 Column = Index % Columns;
        const int32 Row = Index / Columns;

        Slot->SetRelativeLocation(FVector(
            (Column - 1.5f) * Spacing,
            (Row - 1.0f) * Spacing,
            5.0f));

        FoodSlots.Add(Slot);
    }
}

int32 ABuffetStation::TryAddFood(AFoodItemActor* Food, FGuid ExpectedLeaseId)
{
    if (bUpdatingStock
        || !IsValid(Food)
        || !Food->MatchesLease(ExpectedLeaseId)
        || Food->IsCarried())
    {
        return 0;
    }

    UFoodSnapSlotComponent* Slot = FindEmptySlot(Food);

    if (!Slot)
    {
        return 0;
    }

    TGuardValue<bool> StockGuard(bUpdatingStock, true);

    return Slot->TryPlaceFood(Food) ? 1 : 0;
}

bool ABuffetStation::HasFood() const
{
    return FindOccupiedSlot() != nullptr;
}

/*
음식 슬롯 조회
*/
int32 ABuffetStation::GetRemainingServings() const
{
    int32 Count = 0;

    for (const TObjectPtr<UFoodSnapSlotComponent>& SlotRef : FoodSlots)
    {
        UFoodSnapSlotComponent* Slot = SlotRef.Get();
        AFoodItemActor* Food = IsValid(Slot) ? Slot->GetFood() : nullptr;

        if (IsValid(Food) && Food->IsReadyToEat())
        {
            ++Count;
        }
    }

    return Count;
}

UFoodDefinition* ABuffetStation::GetFoodDefinition() const
{
    UFoodSnapSlotComponent* Slot = FindOccupiedSlot();
    AFoodItemActor* Food = Slot ? Slot->GetFood() : nullptr;

    return IsValid(Food) ? Food->GetFoodDefinition() : nullptr;
}

UFoodSnapSlotComponent* ABuffetStation::FindOccupiedSlot() const
{
    for (const TObjectPtr<UFoodSnapSlotComponent>& SlotRef : FoodSlots)
    {
        UFoodSnapSlotComponent* Slot = SlotRef.Get();
        AFoodItemActor* Food = IsValid(Slot) ? Slot->GetFood() : nullptr;

        if (IsValid(Food) && Food->IsReadyToEat())
        {
            return Slot;
        }
    }

    return nullptr;
}

UFoodSnapSlotComponent* ABuffetStation::FindEmptySlot(
    const AFoodItemActor* Food) const
{
    if (!IsValid(Food)
        || !Food->IsReadyToEat()
        || Food->GetWorld() != GetWorld())
    {
        return nullptr;
    }

    // 한 트레이에는 같은 종류의 음식만 담는다.
    UFoodDefinition* ExistingDefinition = GetFoodDefinition();

    if (ExistingDefinition
        && ExistingDefinition != Food->GetFoodDefinition())
    {
        return nullptr;
    }

    for (const TObjectPtr<UFoodSnapSlotComponent>& SlotRef : FoodSlots)
    {
        UFoodSnapSlotComponent* Slot = SlotRef.Get();

        if (IsValid(Slot) && Slot->CanAccept(Food))
        {
            return Slot;
        }
    }

    return nullptr;
}

/*
Interact : E키
*/
bool ABuffetStation::CanInteract_Implementation(AActor* Interactor) const
{
    return !bUpdatingStock
        && FindEmptySlot(FoodPlacement::GetHeldFood(Interactor)) != nullptr;
}

bool ABuffetStation::CanInteractWhileCarrying_Implementation(AActor* Interactor) const
{
    return CanInteract_Implementation(Interactor);
}

FText ABuffetStation::GetInteractPrompt_Implementation(AActor* Interactor) const
{
    return NSLOCTEXT("Food", "PlaceOnBuffet", "트레이에 음식 옮기기");
}

void ABuffetStation::Interact_Implementation(AActor* Interactor)
{
    if (bUpdatingStock)
    {
        return;
    }

    UFoodSnapSlotComponent* Slot = FindEmptySlot(
        FoodPlacement::GetHeldFood(Interactor));

    if (!Slot)
    {
        return;
    }

    TGuardValue<bool> StockGuard(bUpdatingStock, true);

    // 접시는 계속 들고 있고 음식만 빈 슬롯으로 옮긴다.
    FoodPlacement::TryPlaceHeldFood(Interactor, Slot);
}

bool ABuffetStation::TryReserveServingPoint(AActor* Requester, FGuid RequestId)
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

bool ABuffetStation::OwnsReservation(AActor* Requester, FGuid RequestId) const
{
    return IsValid(Requester)
        && !Requester->IsActorBeingDestroyed()
        && ReservedGuest.Get() == Requester
        && RequestId.IsValid()
        && ReservationId == RequestId;
}

void ABuffetStation::ReleaseServingPoint(AActor* Requester, FGuid RequestId)
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

AFoodPlateActor* ABuffetStation::TryTakeServing(
    AActor* Requester,
    FGuid RequestId,
    const FTransform& PlateTransform)
{
    if (bUpdatingStock
        || !OwnsReservation(Requester, RequestId)
        || Requester->GetWorld() != GetWorld()
        || !IsValid(Requester->GetRootComponent())
        || !GuestPlateClass
        || PlateTransform.ContainsNaN())
    {
        return nullptr;
    }

    UFoodSnapSlotComponent* SourceSlot = FindOccupiedSlot();
    AFoodItemActor* Food = SourceSlot ? SourceSlot->GetFood() : nullptr;

    if (!IsValid(Food))
    {
        return nullptr;
    }

    TGuardValue<bool> StockGuard(bUpdatingStock, true);

    FActorSpawnParameters Params;
    Params.Owner = Requester;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;

    AFoodPlateActor* Plate = GetWorld()->SpawnActor<AFoodPlateActor>(
        GuestPlateClass,
        PlateTransform,
        Params);

    if (!IsValid(Plate))
    {
        return nullptr;
    }

    Plate->SetGuestOwned();

    // 접시 부착에 실패하면 음식은 트레이에 남는다.
    if (!Plate->AttachToComponent(
        Requester->GetRootComponent(),
        FAttachmentTransformRules::KeepWorldTransform))
    {
        Plate->Destroy();
        return nullptr;
    }

    UFoodSnapSlotComponent* DestinationSlot = Plate->GetFoodSlot();

    if (!IsValid(DestinationSlot)
        || !DestinationSlot->TryPlaceFood(Food))
    {
        Plate->Destroy();
        return nullptr;
    }

    ReleaseServingPoint(Requester, RequestId);

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


