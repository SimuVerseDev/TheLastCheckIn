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
class ARoomActor;
class AGuestPoolManager;
class AGuestCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGuestRoomAssigned, AGuestCharacter*, Guest);

UCLASS()
class THELASTCHECKIN_API AGuestCharacter : public ACharacter, public IInteractable
{
	GENERATED_BODY()

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
	
	//객실을 배정받고 해당 객실로 이동 요청
	UFUNCTION(BlueprintCallable, Category = "Guests|Room")
	bool TryAssignRoom(ARoomActor* NewRoom);
	
	UFUNCTION(BlueprintCallable, Category = "Guests|AI")
	bool NotifyDestinationReached(AActor* ReachedTarget);
	
	UFUNCTION(BlueprintPure, Category = "Guests|State")
	EGuestState GetGuestState() const
	{
		return GuestState;
	}
	
	//퇴실을 시작하고 로비 지점으로 이동
	UFUNCTION(BlueprintCallable, Category ="Guests|Checkout")
	bool TryBeginCheckout(AActor* LobbyTarget);
	
	//로비에서 출구로 이동 시작
	UFUNCTION(BlueprintCallable, Category="Guests|Checkout")
	bool TryBeginExit(AActor* ExitTarget);
	
	// 퇴실 시각 도달 시 호출
	UFUNCTION(BlueprintCallable, Category = "Guests|Checkout")
	bool HandleCheckoutTime(AActor* LobbyTarget);
	
	//플레이어가 잔류손님을 내쫓을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Guests|Checkout")
	bool RequestCheckout(AActor* LobbyTarget);
	
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
	
	UPROPERTY(BlueprintAssignable, Category ="Guests|Events")
	FGuestRoomAssigned OnGuestRoomAssigned;
	
	FGuid GetStayId() const
	{
		return StayId;
	}
	
	// 잔류 손님 상호작용
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category="Guest|Data")
	FDataTableRowHandle GuestTypeRow;
	
	//손님 행동트리
	UPROPERTY(EditAnywhere, Category="Guest|AI")
	TObjectPtr<UBehaviorTree> GuestBehaviorTree;
	
	UPROPERTY(EditAnywhere, Category="Guest|AI")
	TObjectPtr<AActor> InitialMoveTarget;
	
	//현재 배정된 객실
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Guests|Room")
	TObjectPtr<ARoomActor> AssignedRoom;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Guests|Room")
	FGuid StayId;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Guests|State")
	EGuestState GuestState = EGuestState::MovingToCounter;
	
	//퇴실에서 도착해야 할 로비 지점
	UPROPERTY(VisibleInstanceOnly, BluePrintReadOnly, Category="Guests|Checkout")
	TObjectPtr<AActor> CheckoutLobbyTarget;
	
	UPROPERTY(VisibleInstanceOnly, BluePrintReadOnly, Category="Guests|Checkout")
	TObjectPtr<AActor> CheckoutExitTarget;
	
	// 데이터 테이블에서 읽은 퇴실 정책
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Guests|Checkout")
	EGuestCheckoutPolicy CheckoutPolicy = EGuestCheckoutPolicy::Random;

	// Random 정책에서 사용하는 잔류 확률
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Guests|Checkout")
	float StayBehindChance = 0.0f;

	// 퇴실 시간이 지나도 남을 손님인지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Guests|Checkout")
	bool bWillStayBehind = false;
	
	// 풀용 BP에서 켜면 생성 직후 비활성 상태로 대기
	UPROPERTY(EditDefaultsOnly, Category = "Guests|Pool")
	bool bStartInPool = false;
	

	UPROPERTY()
	TObjectPtr<AGuestPoolManager> PoolManager;

	// 도착 처리가 끝난 후 실행할 반환 예약
	FTimerHandle PoolReturnTimerHandle;

	void ReturnToPool();

};
