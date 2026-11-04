#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HKWipeToolComponent.generated.h"
/*
이 컴포넌트가 붙은 물건은 들고 얼룩을 조준하면 닦기 도구로 동작
*/
UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKWipeToolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	float GetSurfaceOffset() const
	{
		return SurfaceOffset;
	}

	float GetWipeMultiplier() const
	{
		return WipeMultiplier;
	}

protected:
	/*
	닦는 동안 걸레를 표면에서 띄우는 거리 (걸레 두께의 절반 정도)
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe", meta=(ClampMin=0, Units="cm"))
	float SurfaceOffset = 2.0f;

	/*
	움직인 거리에 곱하는 배율 (좋은 걸레일수록 크게)
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe", meta=(ClampMin=0))
	float WipeMultiplier = 1.0f;
};
