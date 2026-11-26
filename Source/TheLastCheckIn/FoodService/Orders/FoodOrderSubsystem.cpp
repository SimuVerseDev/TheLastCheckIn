#include "FoodService/Orders/FoodOrderSubsystem.h"

#include "FoodService/Data/FoodDefinition.h"
#include "FoodService/GuestDiningComponent.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Party/GuestParty.h"

FGuid UFoodOrderSubsystem::CreateRestaurantOrder(
    AGuestCharacter* Guest,
    const FGuid& StayId,
    const FGuid& VisitId,
    UFoodDefinition* Food,
    double CurrentSeconds)
{
    if (!IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetWorld() != GetWorld()
        || !IsValid(Food)
        || Food->FoodId.IsNone()
        || !StayId.IsValid()
        || !VisitId.IsValid())
    {
        return FGuid();
    }

    FFoodOrderRecord Order;
    Order.OrderId = FGuid::NewGuid();
    Order.Guest = Guest;
    Order.StayId = StayId;
    Order.VisitId = VisitId;
    Order.Service = EFoodOrderService::Restaurant;
    Order.CreatedAtSeconds = CurrentSeconds;

    if (!IsCurrentCustomer(Order))
    {
        return FGuid();
    }

    // 같은 방문에서 주문을 중복 생성하지 않는다.
    for (const auto& Pair : Orders)
    {
        const FFoodOrderRecord& Existing = Pair.Value;

        if (Existing.Guest.Get() == Guest
            && Existing.StayId == StayId
            && Existing.VisitId == VisitId
            && Existing.State != EFoodOrderState::Cancelled)
        {
            return Existing.OrderId;
        }
    }

    FFoodOrderLine Line;
    Line.Food = Food;
    Line.Quantity = 1;
    Order.Items.Add(Line);

    const FGuid NewOrderId = Order.OrderId;
    Orders.Add(NewOrderId, Order);

    OnOrdersChanged.Broadcast();
    return NewOrderId;
}

bool UFoodOrderSubsystem::TryAcceptOrder(
    const FGuid& OrderId,
    AGuestCharacter* Guest,
    const FGuid& StayId,
    const FGuid& VisitId)
{
    FFoodOrderRecord* Order = Orders.Find(OrderId);

    if (!Order
        || Order->State != EFoodOrderState::PendingAcceptance
        || Order->Guest.Get() != Guest
        || Order->StayId != StayId
        || Order->VisitId != VisitId
        || !IsCurrentCustomer(*Order))
    {
        return false;
    }

    Order->State = EFoodOrderState::Accepted;

    OnOrdersChanged.Broadcast();
    return true;
}

void UFoodOrderSubsystem::CancelOrder(const FGuid& OrderId, EFoodOrderCancelReason Reason)
{
    FFoodOrderRecord* Order = Orders.Find(OrderId);

    if (!Order || Order->State == EFoodOrderState::Cancelled)
    {
        return;
    }

    Order->State = EFoodOrderState::Cancelled;
    Order->CancelReason = Reason;

    OnOrdersChanged.Broadcast();
}

bool UFoodOrderSubsystem::GetOrder(const FGuid& OrderId, FFoodOrderRecord& OutOrder) const
{
    const FFoodOrderRecord* Order = Orders.Find(OrderId);

    if (!Order)
    {
        OutOrder = FFoodOrderRecord();
        return false;
    }

    OutOrder = *Order;
    return true;
}

TArray<FFoodOrderRecord> UFoodOrderSubsystem::GetOpenOrders() const
{
    TArray<FFoodOrderRecord> Result;

    for (const auto& Pair : Orders)
    {
        const FFoodOrderRecord& Order = Pair.Value;

        if (Order.State != EFoodOrderState::Cancelled
            && IsCurrentCustomer(Order))
        {
            Result.Add(Order);
        }
    }

    Result.Sort(
        [](const FFoodOrderRecord& A, const FFoodOrderRecord& B)
        {
            return A.CreatedAtSeconds < B.CreatedAtSeconds;
        });

    return Result;
}

bool UFoodOrderSubsystem::IsCurrentCustomer(
    const FFoodOrderRecord& Order) const
{
    const AGuestCharacter* Guest = Order.Guest.Get();

    if (!IsValid(Guest)
        || Guest->IsActorBeingDestroyed()
        || Guest->GetWorld() != GetWorld())
    {
        return false;
    }

    const UGuestParty* Party = Guest->GetParty();
    const UGuestDiningComponent* Dining = Guest->GetDiningComponent();

    return IsValid(Party)
        && Party->IsStaying()
        && Party->GetStayId() == Order.StayId
        && IsValid(Dining)
        && Dining->MatchesDinnerVisit(Order.StayId, Order.VisitId);
}

void UFoodOrderSubsystem::Deinitialize()
{
    Orders.Reset();
    OnOrdersChanged.Clear();

    Super::Deinitialize();
}
