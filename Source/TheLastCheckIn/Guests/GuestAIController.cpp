#include "GuestAIController.h"

#include "GuestCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

bool AGuestAIController::SetGuestMoveTarget(AActor* NewTarget)
{
	if (!IsValid(NewTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Move target is invalid."), *GetName());
		
		return false;
	}
	
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
	
	if (BlackboardComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Blackboard component is not initialized."), *GetName());
		
		return false;
	}
	
	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"),NewTarget);
	
	return true;
}

bool AGuestAIController::StartGuestAI(AActor* StartTarget)
{
	AGuestCharacter* Guest = Cast<AGuestCharacter>(GetPawn());

	if (!IsValid(Guest) || !IsValid(StartTarget))
	{
		return false;
	}

	UBehaviorTree* BehaviorTree = Guest->GetGuestBehaviorTree();

	if (!IsValid(BehaviorTree) ||
		!IsValid(BehaviorTree->BlackboardAsset))
	{
		return false;
	}

	// 이전 실행과 이동 요청 정리
	StopGuestAI();

	UBlackboardComponent* BlackboardComponent = nullptr;

	if (!UseBlackboard(
		BehaviorTree->BlackboardAsset,
		BlackboardComponent) ||
		!IsValid(BlackboardComponent))
	{
		return false;
	}

	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), StartTarget);

	if (!RunBehaviorTree(BehaviorTree))
	{
		StopGuestAI();
		return false;
	}

	return true;
}

void AGuestAIController::StopGuestAI()
{
	// 행동 트리가 새로운 이동 요청을 만들지 못하도록 정지
	UBehaviorTreeComponent* BehaviorTreeComponent =
		Cast<UBehaviorTreeComponent>(GetBrainComponent());

	if (IsValid(BehaviorTreeComponent))
	{
		BehaviorTreeComponent->StopTree(EBTStopMode::Forced);
	}

	// 현재 진행 중인 이동 중단
	StopMovement();

	// 이전 목적지 제거
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();

	if (IsValid(BlackboardComponent))
	{
		BlackboardComponent->ClearValue(TEXT("TargetActor"));
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}

void AGuestAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	AGuestCharacter* Guest = Cast<AGuestCharacter>(InPawn);
	
	if (!IsValid(Guest))
	{
		return;
	}
	
	// 풀용 손님은 ActivateFromPool에서 AI를 시작
	if (Guest->ShouldStartInPool())
	{
		return;
	}

	
	StartGuestAI(Guest->GetInitialMoveTarget());
	
	
}
