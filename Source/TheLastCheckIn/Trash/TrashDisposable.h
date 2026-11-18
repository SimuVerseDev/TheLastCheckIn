#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TrashDisposable.generated.h"

class ATrashCanActor;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UTrashDisposable : public UInterface
{
	GENERATED_BODY()
};

/*
쓰레기통(ATrashCanActor)에 버릴 수 있는 것 - 쓰레기봉투, 추후 청소 카트에 쌓인 쓰레기 등
*/
class THELASTCHECKIN_API ITrashDisposable
{
	GENERATED_BODY()

public:
	virtual bool CanDispose(const ATrashCanActor* TrashCan) const
	{
		return true;
	}

	/*
	쓰레기통에 버려진 직후 호출 - 스스로 정리(Destroy 등)
	*/
	virtual void OnDisposed(ATrashCanActor* TrashCan) = 0;
};
