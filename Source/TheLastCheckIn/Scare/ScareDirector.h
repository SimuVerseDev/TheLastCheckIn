#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "ScareDirector.generated.h"

class AScareBase;

USTRUCT(BlueprintType)
struct FScareEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) int32 Day = 2;
	UPROPERTY(EditAnywhere) EHotelPhase Phase = EHotelPhase::Day;
	UPROPERTY(EditAnywhere) EDaySlot Slot = EDaySlot::Midday;

	UPROPERTY(EditAnywhere) float MinDelay = 10.f;
	UPROPERTY(EditAnywhere) float MaxDelay = 60.f;
	UPROPERTY(EditAnywhere) TObjectPtr<AScareBase> Scare;

	bool bFired = false;
};

UCLASS()
class THELASTCHECKIN_API AScareDirector : public AActor
{
	GENERATED_BODY()
public:
	AScareDirector();

	UPROPERTY(EditAnywhere, Category = "Scare")
	TArray<FScareEntry> Schedule;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);
	UFUNCTION() void HandleSlotChanged(EDaySlot OldSlot, EDaySlot NewSlot);
	void TryFire();
};