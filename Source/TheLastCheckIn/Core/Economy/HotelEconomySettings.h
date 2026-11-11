#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HotelEconomySettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Hotel Economy"))
class THELASTCHECKIN_API UHotelEconomySettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="New Game", meta=(ClampMin="0"))
	float StartingCapital = 100.0f;
};
