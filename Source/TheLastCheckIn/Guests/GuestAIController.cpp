#include "GuestAIController.h"

#include "GuestCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "CheckIn/CheckInCounter.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "NavigationSystem.h"


AGuestAIController::AGuestAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<
		UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
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

    StopGuestAI();

    UBlackboardComponent* BlackboardComponent = nullptr;

    if (!UseBlackboard(
            BehaviorTree->BlackboardAsset,
            BlackboardComponent) ||
        !IsValid(BlackboardComponent))
    {
        return false;
    }

    const bool bUseCounterSlot =
        Guest->GetGuestState() == EGuestState::MovingToCounter;

    BlackboardComponent->SetValueAsObject(
        TEXT("TargetActor"), StartTarget);

    BlackboardComponent->SetValueAsBool(
        TEXT("UseCounterSlot"), bUseCounterSlot);

    if (bUseCounterSlot)
    {
        const ACheckInCounter* Counter =
            Cast<ACheckInCounter>(StartTarget);

        FTransform Slot;

        if (!IsValid(Counter) ||
            !Counter->GetSlotTransformForGuest(Guest, Slot))
        {
            return false;
        }

        UNavigationSystemV1* Nav =
            FNavigationSystem::GetCurrent<UNavigationSystemV1>(
                GetWorld());

        FNavLocation ProjectedLocation;

        if (!Nav ||
            !Nav->ProjectPointToNavigation(
                Slot.GetLocation(),
                ProjectedLocation,
                FVector(30.0, 30.0, 200.0),
                &Guest->GetNavAgentPropertiesRef()))
        {
            return false;
        }

        // 고정 자리가 옆으로 크게 밀린 경우 사용하지 않음.
        if (FVector::DistSquared2D(
                Slot.GetLocation(),
                ProjectedLocation.Location) > FMath::Square(10.0))
        {
            return false;
        }

        BlackboardComponent->SetValueAsVector(
            TEXT("MoveLocation"),
            ProjectedLocation.Location);
    }

    if (UCrowdFollowingComponent* Crowd =
        Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
    {
        Crowd->SetCrowdSimulationState(
            ECrowdSimulationState::Enabled);
    }

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
	
	if (UCrowdFollowingComponent* Crowd =
	Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
	}
	
	if (AGuestCharacter* Guest = Cast<AGuestCharacter>(GetPawn()))
	{
		Guest->ReleaseMoveLocation();
	}

	// 이전 목적지 제거
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();

	if (IsValid(BlackboardComponent))
	{
		BlackboardComponent->ClearValue(TEXT("TargetActor"));
		BlackboardComponent->ClearValue(TEXT("MoveLocation"));
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
	
	// AI는 풀 매니저가 이동 목적지 설정과 일행/카운터 등록을 마친 뒤 시작한다.
	StopGuestAI();
	if (!Guest->ShouldStartInPool())
	{
		//비풀 손님 미지원 경고 로그 추가
		UE_LOG(LogTemp, Warning,
			TEXT("Guest %s: Non-pooled guests (bStartInPool=false), including level-placed guests, "
				"are not supported. AI will remain stopped. Use a guest class with bStartInPool=true "
				"and spawn parties through GuestPoolManager::TryAcquireParty to initialize "
				"the move target, party, and counter registration before starting AI."),
			*GetNameSafe(Guest));
	}
}

void AGuestAIController::UseWaitingAvoidance()
{
	if (UCrowdFollowingComponent* Crowd =
		Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::ObstacleOnly);
	}
}
