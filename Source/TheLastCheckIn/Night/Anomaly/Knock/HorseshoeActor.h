#pragma once

#include "CoreMinimal.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "HorseshoeActor.generated.h"

UCLASS()
class THELASTCHECKIN_API AHorseshoeActor : public AHKPhysicsPickupActor
{
	GENERATED_BODY()

public:
	void MarkUsed()
	{
		bUsed = true;
		bCanBePickedUp = false;
	}

	UFUNCTION(BlueprintPure, Category="Night|Knock")
	bool IsUsed() const
	{
		return bUsed;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyReleased(bool bThrown) override;

private:
	UFUNCTION()
	void HandleDayStarted(int32 Day);

	bool bUsed = false;
};