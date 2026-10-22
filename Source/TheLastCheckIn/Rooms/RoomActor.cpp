#include "RoomActor.h"

#include "Components/SceneComponent.h"


ARoomActor::ARoomActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	
}

int32 ARoomActor::GetCapacity() const
{
	if (RoomTypeRow.DataTable == nullptr ||
	RoomTypeRow.RowName.IsNone())
	{
		return 0;
	}
	
	const FRoomTypeRow* RoomData =
	RoomTypeRow.GetRow<FRoomTypeRow>(TEXT("ARoomActor::GetCapacity"));

	if (RoomData == nullptr)
	{
		return 0;
	}

	return FMath::Max(0, RoomData->Capacity);
}

bool ARoomActor::TryReserve(const FGuid& InStayId)
{
	if (!InStayId.IsValid())
	{
		return false;
	}
	
	//이미 배정되었거나 투숙 중인 객실은 사용불가
	if (RoomState != ERoomState::Available||
		CurrentStayId.IsValid())
	{
		return false;
	}
	
	if (GetCapacity()<1||
		!IsValid(ArrivalTarget.Get()))
	{
		return false;
	}
	
	CurrentStayId = InStayId;
	RoomState = ERoomState::Assigned;
	
	return true;
}

bool ARoomActor::CancelReservation(const FGuid& InStayId)
{
	if (!InStayId.IsValid()||
		CurrentStayId!=InStayId||
		RoomState!=ERoomState::Assigned)
	{
		return false;
	}
	
	CurrentStayId.Invalidate();
	RoomState = ERoomState::Available;
	
	return true;
}

bool ARoomActor::MarkOccupied(const FGuid& InStayId)
{
	if (!InStayId.IsValid() ||
	CurrentStayId != InStayId ||
	RoomState != ERoomState::Assigned)
	{
		return false;
	}

	RoomState = ERoomState::Occupied;

	return true;
}


