#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HKRoomVolume.generated.h"
class AHKRoomVolume;
class AHKTrashActor;
class ARoomActor;
class UBoxComponent;
class UHKDirtComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHKOnCleanlinessChanged, AHKRoomVolume*, Room, float, Cleanliness);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnRoomCleaned, AHKRoomVolume*, Room);

UCLASS()
class AHKRoomVolume : public AActor
{
	GENERATED_BODY()

public:
	AHKRoomVolume();
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Room")
	void ApplyRandomDirt();
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Room")
	void ClearDirt();
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Room")
	float GetCleanliness() const
	{
		return Cleanliness;
	}
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Room")
	bool IsDirty() const
	{
		return bDirty;
	}
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Room")
	ARoomActor* GetLinkedRoom() const
	{
		return LinkedRoom;
	}
	
	bool IsPointInside(const FVector& WorldLocation) const;
	
	UPROPERTY(BlueprintAssignable, Category="Housekeeping|Room")
	FHKOnCleanlinessChanged OnCleanlinessChanged;
	UPROPERTY(BlueprintAssignable, Category="Housekeeping|Room")
	FHKOnRoomCleaned OnRoomCleaned;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> Bounds;
	
	// ARoomActor 연결 - 퇴실 연동용
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Housekeeping|Room")
	TObjectPtr<ARoomActor> LinkedRoom;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Room|Trash")
	TArray<TSubclassOf<AHKTrashActor>> TrashClasses;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Room|Dirt", meta=(ClampMin=0))
	int32 MinDirtSources = 1;
	UPROPERTY(EditAnywhere, Category="Housekeeping|Room|Dirt", meta=(ClampMin=0))
	int32 MaxDirtSources = 3;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Room|Trash", meta=(ClampMin=0))
	int32 MinTrash = 2;
	UPROPERTY(EditAnywhere, Category="Housekeeping|Room|Trash", meta=(ClampMin=0))
	int32 MaxTrash = 5;
	
	UPROPERTY(EditInstanceOnly, Category="Housekeeping|Room|Trash")
	TArray<TObjectPtr<AActor>> TrashSpawnPoints;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Room", meta=(ClampMin=0.05, Units="s"))
	float EvaluateInterval = 0.25f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Debug")
	bool bDirtyOnBeginPlay = false;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Debug")
	bool bShowDebug = true;
	
private:
	void CollectCandidates(TArray<UHKDirtComponent*>& OutCandidates) const;
	void SpawnTrash(FRandomStream& Random);
	bool FindTrashSpawnTransform(FRandomStream& Random, FTransform& OutTransform) const;
	bool IsCountedHere(const UHKDirtComponent* Dirt) const;
	void Evaluate();
	void FinishCleaning();
	void SetCleanliness(float NewCleanliness);
	
	TArray<TWeakObjectPtr<UHKDirtComponent>> ActiveDirt;
	
	FTimerHandle EvaluateTimer;
	float TotalWeight = 0.0f;
	
	float Cleanliness = 100.0f;
	bool bDirty = false;
};
