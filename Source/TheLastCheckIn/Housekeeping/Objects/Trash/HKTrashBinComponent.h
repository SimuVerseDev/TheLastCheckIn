// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HKTrashBinComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FHKTrashBinFillChanged,
	int32, CurrentAmount,
	int32, MaxCapacity,
	float, FillRatio);

UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UHKTrashBinComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHKTrashBinComponent();

	// 지정한 양을 추가로 넣을 수 있는지 확인합니다.
	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	bool CanAcceptTrash(int32 Amount = 1) const;

	// 용량이 충분할 때만 추가합니다.
	// 성공하면 true, 가득 찼거나 잘못된 양이면 false를 반환합니다.
	UFUNCTION(BlueprintCallable, Category="Housekeeping|TrashBin")
	bool TryAddTrash(int32 Amount = 1);

	// 쓰레기통을 비우고, 비운 양을 반환합니다.
	UFUNCTION(BlueprintCallable, Category="Housekeeping|TrashBin")
	int32 Empty();

	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	int32 GetCurrentAmount() const
	{
		return CurrentAmount;
	}

	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	int32 GetMaxCapacity() const
	{
		return FMath::Max(1, MaxCapacity);
	}

	// 0.0 = 비어 있음, 0.5 = 절반, 1.0 = 가득 참
	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	float GetFillRatio() const;

	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	bool IsFull() const
	{
		return CurrentAmount >= GetMaxCapacity();
	}

	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	bool IsEmpty() const
	{
		return CurrentAmount == 0;
	}

	// 채워진 메시를 표시할 조건입니다.
	UFUNCTION(BlueprintPure, Category="Housekeeping|TrashBin")
	bool IsHalfFull() const
	{
		return GetFillRatio() >= 0.5f;
	}

	// BP에서 이 이벤트를 받아 메시를 바꿀 수 있습니다.
	UPROPERTY(BlueprintAssignable, Category="Housekeeping|TrashBin")
	FHKTrashBinFillChanged OnFillChanged;

protected:
	virtual void BeginPlay() override;

	// 쓰레기통마다 에디터에서 설정할 최대 용량입니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Housekeeping|TrashBin",
		meta=(ClampMin="1", UIMin="1"))
	int32 MaxCapacity = 10;

private:
	void NotifyFillChanged();

	// 플레이 중에만 사용하는 값입니다.
	// 카트에 돌려놓아도 같은 액터라면 유지됩니다.
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Housekeeping|TrashBin")
	int32 CurrentAmount = 0;
};