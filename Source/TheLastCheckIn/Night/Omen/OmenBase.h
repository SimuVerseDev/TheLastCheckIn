#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OmenBase.generated.h"

UCLASS(Abstract, Blueprintable)
class THELASTCHECKIN_API AOmenBase : public AActor
{
	GENERATED_BODY()

public:
	AOmenBase();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Night|Omen")
	void PlayOmen();

	FName GetRelatedAnomalyId() const
	{
		return RelatedAnomalyId;
	}

protected:
	UPROPERTY(EditAnywhere, Category="Night|Omen")
	FName RelatedAnomalyId;
};