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

	/*
	이 도구를 들고 있는 사람 - 카트 BP의 BeginCarry/EndCarry에서 설정
	*/
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Wipe")
	void SetUser(AActor* NewUser)
	{
		User = NewUser;
	}

	UFUNCTION(BlueprintPure, Category="Housekeeping|Wipe")
	AActor* GetUser() const
	{
		return User.Get();
	}

	/*
	Interactor가 들고 있는 닦기 도구를 찾음 (없으면 nullptr)
	*/
	static UHKWipeToolComponent* FindHeldBy(const AActor* Interactor);

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

private:
	TWeakObjectPtr<AActor> User;
};
