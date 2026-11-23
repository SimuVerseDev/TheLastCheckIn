#include "Trash/TrashCanActor.h"
#include "Trash/TrashDisposable.h"
#include "Components/StaticMeshComponent.h"

ATrashCanActor::ATrashCanActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

bool ATrashCanActor::CanDispose(AActor* Item) const
{
	const ITrashDisposable* Disposable = Cast<ITrashDisposable>(Item);
	return IsValid(Item) && Disposable && Disposable->CanDispose(this);
}

bool ATrashCanActor::TryDispose(AActor* Item)
{
	if (!CanDispose(Item))
	{
		return false;
	}

	/*
	OnDisposed에서 Item이 스스로 Destroy할 수 있으니 알림을 먼저 보냄
	*/
	OnDisposed(Item);
	OnTrashDisposed.Broadcast(this, Item);
	Cast<ITrashDisposable>(Item)->OnDisposed(this);
	return true;
}
