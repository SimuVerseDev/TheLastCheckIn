// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "Data/GuestDataTypes.h"
#include "GuestCharacter.generated.h"

class UBehaviorTree;
class ARoomActor;

UCLASS()
class THELASTCHECKIN_API AGuestCharacter : public ACharacter
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

};
