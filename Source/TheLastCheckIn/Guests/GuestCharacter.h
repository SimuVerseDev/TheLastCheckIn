// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/DataTable.h"
#include "GuestCharacter.generated.h"

class UBehaviorTree;

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

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category="Guest|Data")
	FDataTableRowHandle GuestTypeRow;
	
	//손님 행동트리
	UPROPERTY(EditAnywhere, Category="Guest|AI")
	TObjectPtr<UBehaviorTree> GuestBehaviorTree;
	
	UPROPERTY(EditAnywhere, Category="Guest|AI")
	TObjectPtr<AActor> InitialMoveTarget;

};
