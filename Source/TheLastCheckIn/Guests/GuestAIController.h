#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GuestAIController.generated.h"

UCLASS()
class THELASTCHECKIN_API AGuestAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
};