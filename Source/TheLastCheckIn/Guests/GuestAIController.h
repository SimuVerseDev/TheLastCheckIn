#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Engine/TimerHandle.h"
#include "GuestAIController.generated.h"

class UBrainComponent;

UCLASS()
class THELASTCHECKIN_API AGuestAIController : public AAIController
{
	GENERATED_BODY()
public:
	AGuestAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 이전 이동을 정리하고 새 목적지로 행동 트리를 시작한다.
	bool StartGuestAI(AActor* StartTarget);

	// 행동 트리, 이동, 자리 예약과 복구 타이머를 정리한다.
	void StopGuestAI();

	// 도착한 손님을 다른 손님이 피할 수 있는 대기 장애물로 전환한다.
	void UseWaitingAvoidance();
	
	// 동작 중 AI 일시정지
	void PauseForAction();

	// 동작으로 정지한 AI 재개
	void ResumeAfterAction();

	virtual FAIRequestID RequestMove(
		const FAIMoveRequest& MoveRequest,
		FNavPathSharedPtr Path) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 이동 중에만 주기적으로 도착 직전 정체를 확인한다.
	void CheckFinalApproach();

	FTimerHandle FinalApproachTimerHandle;
	double NearGoalStallStartedAt = -1.0;
	
	// 재개할 이동 요청과 행동 트리
	FAIRequestID ActionPausedMoveId = FAIRequestID::InvalidRequest;
	TWeakObjectPtr<UBrainComponent> ActionPausedBrain;
};
