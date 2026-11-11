#pragma once

#include "CoreMinimal.h"
#include "ReputationTypes.generated.h"

UENUM(BlueprintType)
enum class EReputationReason : uint8
{
	CheckoutSingle,
	CheckoutDouble,
	CleanRoomAssigned,
	DirtyRoomAssigned,
	NeighborAnomalyGuest,
	Complaint,
	ForcedCheckout,
	AnomalyGuestHandled,
	PossessionCleared,
	NormalGuestAttacked,
};