#pragma once

#include "CoreMinimal.h"

class ARoomActor;
class UWorld;

namespace RoomQuery
{
	THELASTCHECKIN_API void GetRooms(const UWorld* World, TArray<ARoomActor*>& OutRooms);

	THELASTCHECKIN_API ARoomActor* FindRoom(const UWorld* World, int32 RoomNumber);
}
