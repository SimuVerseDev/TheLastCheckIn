#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "GuestPoolManager.generated.h"

class AGuestCharacter;

// 입장 처리 결과. 일정 관리자가 재시도 여부를 판단할 때 사용한다.
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

	// 최대 일행 인원을 수용할 수 있는 입장 설정인지 검사한다.
	bool ValidatePartyConfiguration(
		const TArray<AActor*>& SpawnPoints,
		AActor* CounterTarget,
		const FDataTableRowHandle& GuestTypeRow,
		int32 MaxPartySize) const;

	// 일행을 활성화하고 카운터에 등록한다. 실패 시 활성화를 취소하고 원인을 반환한다.
	EGuestAcquireResult TryAcquireParty(
		const TArray<AActor*>& SpawnPoints,
		AActor* CounterTarget,
		const FDataTableRowHandle& GuestTypeRow,
		int32 PartySize,
		TArray<AGuestCharacter*>& OutMembers);

	// 출구에 도착한 손님을 풀의 대기 목록으로 반환한다.
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

	// 풀 대기 설정이 활성화된 손님 클래스.
	UPROPERTY(EditAnywhere, Category = "Guests|Pool")
	TSubclassOf<AGuestCharacter> GuestClass;

	// 게임 시작 시 미리 생성할 손님 수.
	UPROPERTY(EditAnywhere, Category = "Guests|Pool",
		meta = (ClampMin = "1"))
	int32 InitialPoolSize = 100;

	// 비활성 상태로 다음 입장을 기다리는 손님.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Guests|Pool")
	TArray<TObjectPtr<AGuestCharacter>> AvailableGuests;

	// 입장부터 퇴실 완료까지 사용 중인 손님.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Guests|Pool")
	TArray<TObjectPtr<AGuestCharacter>> ActiveGuests;

private:
	// 일행을 구성할 때 사용하는 개별 손님 활성화 함수. AI 시작은 일행 등록 후 수행한다.
	AGuestCharacter* AcquireGuestInternal(
		AActor* SpawnPoint,
		AActor* CounterTarget,
		const FDataTableRowHandle& GuestTypeRow);
};
