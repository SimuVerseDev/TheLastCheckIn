#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Trash/TrashDisposable.h"
#include "TrashBagActor.generated.h"

class UStaticMeshComponent;
class ATrashCanActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTrashBagEvent, ATrashBagActor*, Bag);

/*
쓰레기봉투 - 오른손에 들고 다니다가 쓰레기통에 버림
- 든 동안 E: 조준한 곳이 쓰레기통이면 버리기, 아니면 바닥에 내려놓기 (빠루와 같은 방식)
- 바닥에 놓인 봉투는 E로 다시 집기
- 든 동안에는 진행 중인 상호작용이라 E는 봉투로만 가고, LMB는 UTrashBagHolderComponent가 막음
*/
UCLASS()
class THELASTCHECKIN_API ATrashBagActor
	: public AActor
	, public IInteractable
	, public ITrashDisposable
{
	GENERATED_BODY()

public:
	ATrashBagActor();

	/*
	플레이어가 지금 봉투를 새로 들 수 있는지 (다른 걸 들고 있거나 다른 상호작용 중이면 불가)
	*/
	static bool CanHolderTakeBag(AActor* Holder);

	bool TryEquip(AActor* Holder);
	void Drop(bool bEnablePhysics = true);

	UFUNCTION(BlueprintPure, Category="Trash")
	bool IsHeldBy(const AActor* Holder) const;

	UFUNCTION(BlueprintPure, Category="Trash")
	bool IsHeld() const;

	// 쓰레기통에 버려졌을 때 (제거 완료) - Destroy 직전
	UPROPERTY(BlueprintAssignable, Category="Trash")
	FTrashBagEvent OnBagDisposed;

	/*
	IInteractable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;

	/*
	ITrashDisposable
	*/
	virtual bool CanDispose(const ATrashCanActor* TrashCan) const override;
	virtual void OnDisposed(ATrashCanActor* TrashCan) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trash")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 1인칭 메시에서 붙일 소켓 (오른손)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trash")
	FName AttachSocket = NAME_None;

	// 장착 위치 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trash")
	FTransform HandTransform = FTransform::Identity;

	// 든 상태에서 쓰레기통을 인식하는 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trash", meta=(ClampMin="0", Units="cm"))
	float DisposeRange = 250.0f;

	UPROPERTY(EditAnywhere, Category="Trash")
	FText PickupPrompt;

	UPROPERTY(EditAnywhere, Category="Trash")
	FText DropPrompt;

	UPROPERTY(EditAnywhere, Category="Trash")
	FText DisposePrompt;

private:
	ATrashCanActor* FindAimedTrashCan(AActor* Holder) const;
	void ReleaseFromHolder();
};
