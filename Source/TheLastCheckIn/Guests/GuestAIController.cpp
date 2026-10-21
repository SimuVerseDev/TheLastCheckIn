#include "GuestAIController.h"

#include "GuestCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

void AGuestAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	AGuestCharacter* Guest = Cast<AGuestCharacter>(InPawn);
	
	if (Guest == nullptr)
	{
		return;
	}
	
	UBehaviorTree* BehaviorTree = Guest->GetGuestBehaviorTree();
	AActor* MoveTarget = Guest -> GetInitialMoveTarget();
	
	//트리와 목적지가 설정되었는지 확인
	if (BehaviorTree == nullptr ||
		BehaviorTree -> BlackboardAsset == nullptr ||
		MoveTarget == nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[%s] Behavior Tree, Blackboard, or target is missing."),
			*GetName()
		);

		return;
	}
	
	UBlackboardComponent* BlackboardComponent = nullptr;

	if (!UseBlackboard(
		BehaviorTree->BlackboardAsset,
		BlackboardComponent))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[%s] Failed to initialize Blackboard."),
			*GetName()
		);

		return;
	}

	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"),MoveTarget);

	// 목적지를 등록한 뒤 행동 트리를 시작
	if (!RunBehaviorTree(BehaviorTree))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[%s] Failed to start Behavior Tree."),
			*GetName()
		);
	}
	
	
}
