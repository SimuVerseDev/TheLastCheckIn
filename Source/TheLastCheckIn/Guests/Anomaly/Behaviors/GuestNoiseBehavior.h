#pragma once

#include "CoreMinimal.h"
#include "Guests/Anomaly/Behaviors/GuestMoveToPointBehavior.h"
#include "GuestNoiseBehavior.generated.h"

class AGuestActionPoint;

UCLASS()
class THELASTCHECKIN_API UGuestNoiseBehavior
	: public UGuestMoveToPointBehavior
{
	GENERATED_BODY()

public:
	virtual bool CanStart() const override;

protected:
	virtual void OnStarted_Implementation() override;
	virtual void OnStopped_Implementation() override;
	virtual void BeginActing() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AGuestActionPoint> Point;
};