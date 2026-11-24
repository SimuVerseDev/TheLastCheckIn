#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScareBase.generated.h"

UCLASS(Abstract, Blueprintable)
class THELASTCHECKIN_API AScareBase : public AActor
{
	GENERATED_BODY()
public:
	AScareBase();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Scare")
	void PlayScare();
	
	UFUNCTION(CallInEditor, Category = "Scare")
	void TestPlay()
	{
		PlayScare();
	}
};