#pragma once

#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "GuestCrowdFollowingComponent.generated.h"

UCLASS()
class THELASTCHECKIN_API UGuestCrowdFollowingComponent
    : public UCrowdFollowingComponent
{
    GENERATED_BODY()

public:
    UGuestCrowdFollowingComponent(
        const FObjectInitializer& ObjectInitializer
            = FObjectInitializer::Get())
        : Super(ObjectInitializer)
    {
    }

    void SetRequireActualArrival(bool bEnabled)
    {
        bRequireActualArrival = bEnabled;
    }

protected:
    virtual void UpdatePathSegment() override
    {
        if (bRequireActualArrival && IsCrowdSimulationEnabled())
        {
            // Require actual arrival instead of finishing from the movement direction.
            bCheckMovementAngle = false;
            bCanCheckMovingTooFar = false;
        }

        Super::UpdatePathSegment();
    }

private:
    bool bRequireActualArrival = false;
};
