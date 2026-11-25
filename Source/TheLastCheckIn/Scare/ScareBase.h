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

	// 시간표(FAnomalyScheduleRow, Kind = Scare)의 EventId와 같은 ID를 가진 연출이 전부 실행됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	FName ScareId;

	UFUNCTION(BlueprintImplementableEvent, Category = "Scare")
	void PlayScare();
	
	UFUNCTION(CallInEditor, Category = "Scare")
	void TestPlay()
	{
		PlayScare();
	}
};