// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "Data/GuestDataTypes.h"
#include "TimerManager.h"
#include "Core/Interaction/Interactable.h"

#include "GuestCharacter.generated.h"

class UBehaviorTree;
class AGuestPoolManager;
class UGuestParty;

UCLASS()
class THELASTCHECKIN_API AGuestCharacter : public ACharacter, public IInteractable
{
	GENERATED_BODY()
	friend class UGuestParty;
public:
	AGuestCharacter();
	
	bool ApplyGuestData(const FDataTableRowHandle& InGuestTypeRow);
	
	UBehaviorTree* GetGuestBehaviorTree() const
	{
		return GuestBehaviorTree.Get();
	}
	
	AActor* GetInitialMoveTarget() const
	{
		return InitialMoveTarget.Get();
	}
	
	UFUNCTION(BlueprintPure, Category = "Guests|Party")
	UGuestParty* GetParty() const;

	//일행 생성 실패 시 객실 배정 전의 입장을 취소
	bool CancelPoolActivation();
	
	
	UFUNCTION(BlueprintCallable, Category = "Guests|AI")
	bool NotifyDestinationReached(AActor* ReachedTarget);
	
	UFUNCTION(BlueprintPure, Category = "Guests|State")
	EGuestState GetGuestState() const
	{
		return GuestState;
	}


	// 생성 직후 풀에서 대기할 손님인지
	bool ShouldStartInPool() const
	{
		return bStartInPool;
	}

	// 풀에서 꺼내 입장시키기
	bool ActivateFromPool(
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation,
		AActor* CounterTarget,
		const FDataTableRowHandle& InGuestTypeRow);

	// 퇴실이 끝난 손님을 풀 대기 상태로 전환
	bool DeactivateToPool();
	
	void SetPoolManager(AGuestPoolManager* InPoolManager);

	// 잔류 손님 상호작용
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;
	
	// EQS가 탐색할 공통 목적지
	UFUNCTION(BlueprintPure, Category = "Guests|Movement")
	AActor* GetDestinationActor() const
	{
		return MoveTarget.Get();
	}

	// 해당 위치를 사용할 수 있는지 검사
	bool CanReserveMoveLocation(const FVector& Location) const;

	// EQS 결과를 실제로 예약
	UFUNCTION(BlueprintCallable, Category = "Guests|Movement")
	bool TryReserveMoveLocation(const FVector& Location);

	void ReleaseMoveLocation();

	// 행동 트리 Move To의 Acceptable Radius와 맞춰야 함
	static constexpr float MoveAcceptanceRadius = 20.0f;

protected:
	virtual void BeginPlay() override;
	
	// 풀에서 꺼낼 때 새 외형을 준비하도록 BP에 알림
	UFUNCTION(BlueprintImplementableEvent, Category = "Guests|Appearance")
	void OnPrepareGuestAppearance();
	
	// 추가한 얼굴, 헤어 등의 업데이트를 켜거나 끄도록 BP에 알림
	UFUNCTION(BlueprintImplementableEvent, Category = "Guests|Appearance")
	void OnGuestAppearanceActiveChanged(bool bActive);
	
	UPROPERTY(EditAnywhere, Category="Guest|Data")
	FDataTableRowHandle GuestTypeRow;
	
	//손님 행동트리
	UPROPERTY(EditAnywhere, Category="Guest|AI")
	TObjectPtr<UBehaviorTree> GuestBehaviorTree;
	
	UPROPERTY(EditAnywhere, Category="Guest|AI")
	TObjectPtr<AActor> InitialMoveTarget;
	
	// 이 손님이 속한 일행
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Guests|Party")
	TObjectPtr<UGuestParty> Party;
	
	// 일행이 이 손님에게 이동을 지시할 때 사용
	bool BeginMoveTo(AActor* Target, EGuestState MovingState);

	// 일행이 객실 연결을 정리한 후에만 호출하는 비정상 방문 종료.
	void AbortVisitAndReturnToPool();

	UPROPERTY(Transient)
	TObjectPtr<AActor> MoveTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Guests|State")
	EGuestState GuestState = EGuestState::MovingToCounter;

	// 풀용 BP에서 켜면 생성 직후 비활성 상태로 대기
	UPROPERTY(EditDefaultsOnly, Category = "Guests|Pool")
	bool bStartInPool = false;
	

	UPROPERTY()
	TObjectPtr<AGuestPoolManager> PoolManager;

	// 도착 처리가 끝난 후 실행할 반환 예약
	FTimerHandle PoolReturnTimerHandle;

	void ReturnToPool();
	
	UPROPERTY(Transient)
	bool bHasReservedMoveLocation = false;

	UPROPERTY(Transient)
	FVector ReservedMoveLocation = FVector::ZeroVector;

};
