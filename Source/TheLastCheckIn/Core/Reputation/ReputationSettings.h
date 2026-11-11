#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/Reputation/Enums/ReputationTypes.h"
#include "ReputationSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hotel Reputation"))
class THELASTCHECKIN_API UReputationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UReputationSettings* Get() // UReputationSettings::Get()으로 호출가능
	{
		return GetDefault<UReputationSettings>();
	}

	UPROPERTY(Config, EditAnywhere, Category = "Range")
	int32 StartReputation = 50;
	UPROPERTY(Config, EditAnywhere, Category = "Range", meta = (ClampMin = "1"))
	int32 MaxReputation = 1250;
	UPROPERTY(Config, EditAnywhere, Category = "Range", meta = (ClampMin = "1"))
	int32 ReputationPerStar = 250;

	/*
	기본 방값 * StarRateMultipliers
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Room Rate")
	TArray<float> StarRateMultipliers = { 1.0f, 1.1f, 1.2f, 1.35f, 1.5f, 1.7f };

	UPROPERTY(Config, EditAnywhere, Category = "Cleanliness", meta = (ClampMin = "0", ClampMax = "100")) // META : 최소/최대 범위를 강제
	float CleanBonusThreshold = 90.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Cleanliness", meta = (ClampMin = "0", ClampMax = "100"))
	float DirtyPenaltyThreshold = 60.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Guests", meta = (ClampMin = "0", ClampMax = "1"))
	float ForcedCheckoutPenaltyChance = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Deltas")
	TMap<EReputationReason, int32> ReasonDeltas =
	{
		{ EReputationReason::CheckoutSingle, 10 },
		{ EReputationReason::CheckoutDouble, 15 },
		{ EReputationReason::CleanRoomAssigned, 10 },
		{ EReputationReason::DirtyRoomAssigned, -20 },
		{ EReputationReason::NeighborAnomalyGuest, -15 },
		{ EReputationReason::Complaint, -10 },
		{ EReputationReason::ForcedCheckout, -15 },
		{ EReputationReason::AnomalyGuestHandled, 30 },
		{ EReputationReason::PossessionCleared, 25 },
		{ EReputationReason::NormalGuestAttacked, -50 },
	};
};