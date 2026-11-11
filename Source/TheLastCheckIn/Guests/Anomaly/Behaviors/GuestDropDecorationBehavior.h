#pragma once

#include "CoreMinimal.h"
#include "Guests/Anomaly/GuestAnomalyBehavior.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "GuestDropDecorationBehavior.generated.h"

class AGuestAIController;

UCLASS()
class THELASTCHECKIN_API UGuestDropDecorationBehavior
	: public UGuestAnomalyBehavior
{
	GENERATED_BODY()

public:
	virtual bool CanStart() const override;

protected:
	//목적지로 이동
	virtual void OnStarted_Implementation() override;
	//이동, 타이머, 애니메이션 정리
	virtual void OnStopped_Implementation() override;

private:
	void HandleMoveFinished(
		FAIRequestID RequestId,
		const FPathFollowingResult& Result);

	void HandleMoveTimeout();
	void BeginActing();
	//행동 애니메이션 반복 
	void PerformAction();

	UPROPERTY(Transient)
	TObjectPtr<AGuestAIController> GuestController;

	FAIRequestID ActiveMoveId = FAIRequestID::InvalidRequest;
	FDelegateHandle MoveFinishedHandle;

	FTimerHandle MoveTimeoutHandle;
	FTimerHandle ActionTimerHandle;

	bool bActing = false;

	static constexpr float MoveTimeoutSeconds = 30.0f;
	static constexpr float AcceptanceRadius = 40.0f;
};