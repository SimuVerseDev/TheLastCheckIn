#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrashCanActor.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTrashDisposedEvent, ATrashCanActor*, TrashCan, AActor*, Item);

/*
쓰레기통 - ITrashDisposable을 구현한 것을 받아서 버림
손에 든 쪽(쓰레기봉투 등)이 조준한 쓰레기통의 TryDispose를 호출
*/
UCLASS()
class THELASTCHECKIN_API ATrashCanActor : public AActor
{
	GENERATED_BODY()

public:
	ATrashCanActor();

	UFUNCTION(BlueprintPure, Category="Trash")
	bool CanDispose(AActor* Item) const;

	UFUNCTION(BlueprintCallable, Category="Trash")
	bool TryDispose(AActor* Item);

	UPROPERTY(BlueprintAssignable, Category="Trash")
	FTrashDisposedEvent OnTrashDisposed;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trash")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 버리는 소리 등 연출
	UFUNCTION(BlueprintImplementableEvent, Category="Trash")
	void OnDisposed(AActor* Item);
};
