#include "GuestAIController.h"
#include "TheLastCheckIn.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "CheckIn/CheckInCounter.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "GuestCharacter.h"
#include "Guests/GuestCrowdFollowingComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr float FinalApproachCheckInterval = 0.5f;
	constexpr double FinalApproachStallSeconds = 1.0;
	constexpr float StoppedSpeed = 5.0f;
}

AGuestAIController::AGuestAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<
		UGuestCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
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

	const EGuestState State = Guest->GetGuestState();

	const bool bUseCounterSlot = State == EGuestState::MovingToCounter;
	const bool bUseQueuePoint = State == EGuestState::MovingToBuffetQueue;
	const bool bUseSeatPoint = State == EGuestState::MovingToDiningSeat;
	const bool bUseFixedLocation = bUseCounterSlot || bUseQueuePoint || bUseSeatPoint;

	BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), StartTarget);

	// 카운터, 식당 줄, 의자 앞 이동은 지정된 고정 위치를 사용한다.
	BlackboardComponent->SetValueAsBool(TEXT("UseCounterSlot"), bUseFixedLocation);

	if (bUseFixedLocation)
	{
		FTransform Slot;
		if (bUseCounterSlot)
		{
			const ACheckInCounter* Counter = Cast<ACheckInCounter>(StartTarget);
			if (!IsValid(Counter) || !Counter->GetSlotTransformForGuest(Guest, Slot))
			{
				return false;
			}
		}
		else
		{
			const ATargetPoint* QueuePoint = Cast<ATargetPoint>(StartTarget);
			if (!IsValid(QueuePoint))
			{
				return false;
			}
			Slot = QueuePoint->GetActorTransform();
		}

		UNavigationSystemV1* Nav =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

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

	// 카운터의 고정 자리 이동은 기존 방식을 유지한다.
	if (!bUseCounterSlot)
	{
		GetWorldTimerManager().SetTimer(FinalApproachTimerHandle, this,
			&AGuestAIController::CheckFinalApproach, FinalApproachCheckInterval, true);
	}

	PauseForAction();

	return true;
}

void AGuestAIController::StopGuestAI()
{
	GetWorldTimerManager().ClearTimer(FinalApproachTimerHandle);
	NearGoalStallStartedAt = -1.0;

	// 행동 트리를 먼저 정지해 새로운 이동 요청을 막는다.
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
		Crowd->SuspendCrowdSteering(false);
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

	// 종료된 AI의 일시정지 상태 정리
	ResumeAfterAction();
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
		UE_LOG(LogGuests, Warning,
			TEXT("Guest %s: Non-pooled guests (bStartInPool=false), including level-placed guests, "
				"are not supported. AI will remain stopped. Use a guest class with bStartInPool=true "
				"and spawn parties through GuestPoolManager::TryAcquireParty to initialize "
				"the move target, party, and counter registration before starting AI."),
			*GetNameSafe(Guest));
	}
}

void AGuestAIController::UseWaitingAvoidance()
{
	GetWorldTimerManager().ClearTimer(FinalApproachTimerHandle);
	NearGoalStallStartedAt = -1.0;

	if (UCrowdFollowingComponent* Crowd =
		Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SuspendCrowdSteering(false);
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::ObstacleOnly);
	}
}

void AGuestAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FinalApproachTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AGuestAIController::CheckFinalApproach()
{
	AGuestCharacter* Guest = Cast<AGuestCharacter>(GetPawn());
	UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	const UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();

	if (!IsValid(Guest) ||
		!IsValid(Crowd) ||
		!IsValid(BlackboardComponent)
		|| GetMoveStatus() != EPathFollowingStatus::Moving
		||!BlackboardComponent->IsVectorValueSet(TEXT("MoveLocation")))
	{
		NearGoalStallStartedAt = -1.0;
		return;
	}

	const EGuestState State = Guest->GetGuestState();

	if (State != EGuestState::MovingToRoom
		&& State != EGuestState::MovingToLobby
		&& State != EGuestState::MovingToExit
		&& State != EGuestState::MovingToBuffetQueue
		&& State != EGuestState::MovingToDiningSeat
		&& State != EGuestState::MovingToBuffet
		&& State != EGuestState::ReturningFromBuffet)
	{
		NearGoalStallStartedAt = -1.0;
		return;
	}
	const UCapsuleComponent* Capsule = Guest->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = Guest->GetActorLocation();
	const FVector Goal = BlackboardComponent->GetValueAsVector(TEXT("MoveLocation"));
	const double DistanceSquared = FVector::DistSquared2D(Location, Goal);
	const float ApproachRadius = Radius * 2.0f + AGuestCharacter::MoveAcceptanceRadius;

	if (Goal.ContainsNaN() || DistanceSquared > FMath::Square(ApproachRadius) ||
		DistanceSquared <= FMath::Square(AGuestCharacter::MoveAcceptanceRadius) ||
		FMath::Abs(Location.Z - HalfHeight - Goal.Z) > AGuestCharacter::MoveAcceptanceRadius ||
		Guest->GetVelocity().SizeSquared() > FMath::Square(StoppedSpeed))
	{
		NearGoalStallStartedAt = -1.0;
		return;
	}

	// 정체가 관측된 시점부터 월드 시간으로 1초 이상 유지됐는지 확인한다.
	const double Now = GetWorld()->GetTimeSeconds();
	if (NearGoalStallStartedAt < 0.0)
	{
		NearGoalStallStartedAt = Now;
		return;
	}
	if (Now - NearGoalStallStartedAt < FinalApproachStallSeconds)
	{
		return;
	}

	// 목표까지 캡슐이 통과할 수 있을 때만 군중 회피를 해제한다.
	// 실제 캐릭터 충돌과 행동 트리의 도착 판정은 유지한다.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Guest);
	const FVector GoalCenter(Goal.X, Goal.Y, Location.Z);

	if (GetWorld()->SweepTestByProfile(Location, GoalCenter, FQuat::Identity,
		Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
	{
		NearGoalStallStartedAt = -1.0;
		return;
	}

	Crowd->SuspendCrowdSteering(true);
	GetWorldTimerManager().ClearTimer(FinalApproachTimerHandle);
	NearGoalStallStartedAt = -1.0;
}


void AGuestAIController::PauseForAction()
{
	AGuestCharacter* Guest = Cast<AGuestCharacter>(GetPawn());

	if (!IsValid(Guest) || !Guest->IsActionMovementPaused())
	{
		return;
	}

	NearGoalStallStartedAt = -1.0;

	UBrainComponent* Brain = GetBrainComponent();

	if (IsValid(Brain) && Brain->IsRunning() && !Brain->IsPaused())
	{
		ActionPausedBrain = Brain;
		Brain->PauseLogic(TEXT("Guest action"));
	}

	const EPathFollowingStatus::Type Status = GetMoveStatus();

	if (Status == EPathFollowingStatus::Moving ||
		Status == EPathFollowingStatus::Waiting)
	{
		const FAIRequestID MoveId = GetCurrentMoveRequestID();

		if (PauseMove(MoveId))
		{
			ActionPausedMoveId = MoveId;
		}
	}

	Guest->GetCharacterMovement()->StopMovementImmediately();
}

void AGuestAIController::ResumeAfterAction()
{
	const FAIRequestID MoveId = ActionPausedMoveId;
	UBrainComponent* Brain = ActionPausedBrain.Get();

	ActionPausedMoveId = FAIRequestID::InvalidRequest;
	ActionPausedBrain.Reset();
	NearGoalStallStartedAt = -1.0;

	// 일시정지한 이동 요청만 재개
	if (MoveId.IsValid() &&
		GetCurrentMoveRequestID() == MoveId &&
		GetMoveStatus() == EPathFollowingStatus::Paused)
	{
		ResumeMove(MoveId);
	}

	if (IsValid(Brain) && Brain->IsPaused())
	{
		Brain->ResumeLogic(TEXT("Guest action finished"));
	}
}

FAIRequestID AGuestAIController::RequestMove(
	const FAIMoveRequest& MoveRequest,
	FNavPathSharedPtr Path)
{
	const FAIRequestID MoveId =
		Super::RequestMove(MoveRequest, Path);

	// 동작 중 새 이동 요청도 일시정지
	PauseForAction();

	return MoveId;
}
