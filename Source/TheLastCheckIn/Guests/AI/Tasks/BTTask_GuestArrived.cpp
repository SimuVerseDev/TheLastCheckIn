#include "BTTask_GuestArrived.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Guests/GuestCharacter.h"

UBTTask_GuestArrived::UBTTask_GuestArrived(const FObjectInitializer& ObjectInitializer)
{
	NodeName = TEXT("Guest Arrived");
}

EBTNodeResult::Type UBTTask_GuestArrived::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	
	if (!IsValid(AIController) || !IsValid(BlackboardComponent))
	{
		return EBTNodeResult::Failed;
	}
	
	AGuestCharacter *Guest = Cast<AGuestCharacter>(AIController->GetPawn());
	
	if (!IsValid(Guest))
	{
		return EBTNodeResult::Failed;
	}
	
	const FName TargetKey(TEXT("TargetActor"));
	
	AActor* ReachedTarget = Cast<AActor>(
		BlackboardComponent->GetValueAsObject(TargetKey)
	);

	if (!IsValid(ReachedTarget))
	{
		return EBTNodeResult::Failed;
	}

	// 손님에게 도착 사실을 전달하고 상태 변경
	const bool bArrivalHandled =
		Guest->NotifyDestinationReached(ReachedTarget);

	if (!bArrivalHandled)
	{
		return EBTNodeResult::Failed;
	}
	
	//도착 처리가 성공한 경우에만 목적지를 정리
	BlackboardComponent->ClearValue(TargetKey);

	return EBTNodeResult::Succeeded;
}
