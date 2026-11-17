#include "Rooms/RoomQuery.h"
#include "Rooms/RoomActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void RoomQuery::GetRooms(const UWorld* World, TArray<ARoomActor*>& OutRooms)
{
	OutRooms.Reset();

	if (!World)
	{
		return;
	}

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			OutRooms.Add(*It);
		}
	}
}

ARoomActor* RoomQuery::FindRoom(const UWorld* World, int32 RoomNumber)
{
	if (!World)
	{
		return nullptr;
	}

	ARoomActor* FoundRoom = nullptr;

	for (TActorIterator<ARoomActor> It(World); It; ++It)
	{
		ARoomActor* Room = *It;

		if (!IsValid(Room) || Room->GetRoomNumber() != RoomNumber)
		{
			continue;
		}

		if (FoundRoom)
		{
			return nullptr;
		}

		FoundRoom = Room;
	}

	return FoundRoom;
}
