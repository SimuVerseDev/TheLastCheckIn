#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/Reputation/Enums/ReputationTypes.h"
#include "ReputationSettings.generated.h"

class UReputationHUDWidget;
class USoundBase;

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

	/*
	비워두면 C++ 기본 레이아웃 사용
	*/
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UReputationHUDWidget> HUDWidgetClass;

	/*
	평판이 아래에서 위로 이 값을 넘는 순간 재생 (내려갔다 다시 넘으면 또 재생)
	한 번에 여러 개를 넘으면 가장 높은 값의 소리만 재생
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Sound")
	TMap<int32, TSoftObjectPtr<USoundBase>> MilestoneSounds;

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
		{ EReputationReason::LightAnomalyResolved, 30 },
	};
};