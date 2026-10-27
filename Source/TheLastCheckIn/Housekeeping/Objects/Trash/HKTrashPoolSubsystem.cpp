#include "Housekeeping/Objects/Trash/HKTrashPoolSubsystem.h"
#include "Housekeeping/Objects/Trash/HKTrashActor.h"
#include "Engine/World.h"

AHKTrashActor* UHKTrashPoolSubsystem::GetTrash(TSubclassOf<AHKTrashActor> TrashClass, const FTransform& SpawnTransform)
{
	if (!TrashClass)
	{
		return nullptr;
	}

	if (FHKTrashPoolBucket* Bucket = PooledTrash.Find(TrashClass))
	{
		while (Bucket->Items.Num() > 0)
		{
			AHKTrashActor* Trash = Bucket->Items.Pop();
			if (IsValid(Trash))
			{
				Trash->ExitPool(SpawnTransform);
				return Trash;
			}
		}
	}

	// if nothing
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	return GetWorld()->SpawnActor<AHKTrashActor>(TrashClass, SpawnTransform, Params);
}

void UHKTrashPoolSubsystem::ReturnToPool(AHKTrashActor* Trash)
{
	if (!IsValid(Trash) || Trash->IsPooled())
	{
		return; 
	}

	Trash->EnterPool(); // 쓰레기가 눈에 보이지않게끔 처리 - DeactivateBullet과 동일
	PooledTrash.FindOrAdd(Trash->GetClass()).Items.Add(Trash); // 동시에 아이템 배열에 쓰레기를 담는다
}

void UHKTrashPoolSubsystem::WarmupPool(TSubclassOf<AHKTrashActor> TrashClass, int32 Count)
{
	if (!TrashClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	const FTransform HiddenTransform(FVector(0.0f, 0.0f, -100000.0f));

	for (int32 I = 0; I < Count; ++I)
	{
		ReturnToPool(GetWorld()->SpawnActor<AHKTrashActor>(TrashClass, HiddenTransform, Params));
	}
}

int32 UHKTrashPoolSubsystem::GetPooledCount() const
{
	int32 Count = 0;
	for (const TPair<TSubclassOf<AHKTrashActor>, FHKTrashPoolBucket>& Pair : PooledTrash)
	{
		Count += Pair.Value.Items.Num();
	}
	return Count;
}
