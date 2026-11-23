#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "TrashBagHolderComponent.generated.h"

class ATrashBagActor;

/*
쓰레기봉투를 든 동안 플레이어의 LMB(물건 집기 등)를 막음
봉투를 처음 들 때 ATrashBagActor가 플레이어에게 런타임으로 붙임 - 캐릭터 BP 수정 불필요
E 입력은 봉투가 진행 중인 상호작용이라 자동으로 봉투로만 감
*/
UCLASS(ClassGroup=(Trash))
class THELASTCHECKIN_API UTrashBagHolderComponent : public UActorComponent, public IInteractionExtension
{
	GENERATED_BODY()

public:
	UTrashBagHolderComponent();

	static UTrashBagHolderComponent* FindOrAdd(AActor* Holder);

	void SetHeldBag(ATrashBagActor* Bag);
	void ClearHeldBag(const ATrashBagActor* Bag);

	ATrashBagActor* GetHeldBag() const;
	bool IsHolding() const;

	/*
	IInteractionExtension
	*/
	virtual bool TryHandleUseStarted() override;
	virtual void GetIgnoredActors(TArray<AActor*>& IgnoredActors) const override;

private:
	TWeakObjectPtr<ATrashBagActor> HeldBag;
};
