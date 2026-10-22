// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_GuestArrived.generated.h"

UCLASS()
class THELASTCHECKIN_API UBTTask_GuestArrived : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_GuestArrived(const FObjectInitializer& ObjectInitializer);
	
protected:
	virtual EBTNodeResult:: Type ExecuteTask
	(UBehaviorTreeComponent& OwnerComp,uint8* NodeMemory) override;
	
};
