// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "GuestPoolManager.generated.h"

class AGuestCharacter;

enum class EGuestAcquireResult : uint8
{
	Success,
	InvalidConfiguration,
	PoolExhausted,
	CounterBusy,
	SpawnFailed
};

UCLASS()
class THELASTCHECKIN_API AGuestPoolManager : public AActor
{
	GENERATED_BODY()

public:
	AGuestPoolManager();
	

	// 실패 원인을 일정 관리자에게 전달. 기존 AcquireParty 호출도 유지.
	EGuestAcquireResult TryAcquireParty(
		const TArray<AActor*>& SpawnPoints, AActor* CounterTarget,
		const FDataTableRowHandle& GuestTypeRow, int32 PartySize,
		TArray<AGuestCharacter*>& OutMembers);

	bool ValidatePartyConfiguration(
		const TArray<AActor*>& SpawnPoints, AActor* CounterTarget,
		const FDataTableRowHandle& GuestTypeRow, int32 MaxPartySize) const;

	// 퇴실한 손님 반환
	UFUNCTION(BlueprintCallable, Category = "Guests|Pool")
	bool ReleaseGuest(AGuestCharacter* Guest);

	UFUNCTION(BlueprintPure, Category = "Guests|Pool")
	int32 GetAvailableGuestCount() const
	{
		return AvailableGuests.Num();
	}

	UFUNCTION(BlueprintPure, Category = "Guests|Pool")
	int32 GetActiveGuestCount() const
	{
		return ActiveGuests.Num();
	}

protected:
	virtual void BeginPlay() override;

	// 미리 생성할 손님 BP
	UPROPERTY(EditAnywhere, Category = "Guests|Pool")
	TSubclassOf<AGuestCharacter> GuestClass;

	UPROPERTY(EditAnywhere, Category = "Guests|Pool",
		meta = (ClampMin = "1"))
	int32 InitialPoolSize = 100;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Guests|Pool")
	TArray<TObjectPtr<AGuestCharacter>> AvailableGuests;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Guests|Pool")
	TArray<TObjectPtr<AGuestCharacter>> ActiveGuests;
	
private:
	// 일행 구성 중에만 사용하는 개별 NPC 활성화 함수
	AGuestCharacter* AcquireGuestInternal(
		AActor* SpawnPoint,
		AActor* CounterTarget,
		const FDataTableRowHandle& GuestTypeRow);
};
