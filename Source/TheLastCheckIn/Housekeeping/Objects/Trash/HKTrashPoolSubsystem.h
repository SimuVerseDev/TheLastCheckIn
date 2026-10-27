#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HKTrashPoolSubsystem.generated.h"

class AHKTrashActor;

USTRUCT()
struct FHKTrashPoolBucket
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHKTrashActor>> Items;
};

UCLASS()
class UHKTrashPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Trash|Pool")
	AHKTrashActor* GetTrash(TSubclassOf<AHKTrashActor> TrashClass, const FTransform& SpawnTransform);
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Trash|Pool")
	void ReturnToPool(AHKTrashActor* Trash);
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Trash|Pool")
	void WarmupPool(TSubclassOf<AHKTrashActor> TrashClass, int32 Count);

	UFUNCTION(BlueprintPure, Category="Housekeeping|Trash|Pool")
	int32 GetPooledCount() const;

private:
	UPROPERTY(Transient)
	TMap<TSubclassOf<AHKTrashActor>, FHKTrashPoolBucket> PooledTrash;
};
