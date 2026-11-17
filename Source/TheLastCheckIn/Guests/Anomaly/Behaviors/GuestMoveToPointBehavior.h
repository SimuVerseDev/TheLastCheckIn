#pragma once

#include "CoreMinimal.h"
#include "Guests/Anomaly/GuestAnomalyBehavior.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "GuestMoveToPointBehavior.generated.h"

class AGuestAIController;

// 행동 지점까지의 이동 처리를 공유하는 부모
UCLASS(Abstract)
class THELASTCHECKIN_API UGuestMoveToPointBehavior
	: public UGuestAnomalyBehavior
{
	GENERATED_BODY()

public:
	virtual bool CanStart() const override;

protected:
	// 이동에 사용할 컨트롤러를 확보하고 기본 대상을 검사
	bool InitializeMovement();

	// 이동 및 도착 처리에 필요한 대상이 유효한지 검사
	bool HasMovementContext() const;

	// ActionTransform에 저장된 위치로 이동
	void MoveToActionPoint();

	// 이동 완료 후 자식의 기존 행동 시작 함수 호출
	virtual void BeginActing() {}

	// 요청을 정리하고 실제 발 위치가 도착 범위 안인지 검사
	bool TryConfirmArrival(FVector& OutArrivalOffset);

	// 전체 회전을 적용하고 대기 회피 상태로 전환
	void ApplyActionFacing();

	// 이동 요청 식별값과 제한 시간 정리
	void ClearMoveRequest();

	// 완료 콜백 해제,  이동 정지,  군중 설정 복원
	void StopPointMovement();

	virtual void OnSignalReceived_Implementation(
		FName Signal,
		AActor* InstigatorActor) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient)
	TObjectPtr<AGuestAIController> GuestController;

	
	FTransform ActionTransform = FTransform::Identity;

	// 실제 도착 확인과 행동 준비가 끝난 뒤에만 참으로 설정
	bool bActing = false;

private:
	void HandleMoveFinished(
		FAIRequestID RequestId,
		const FPathFollowingResult& Result);

	void HandleMoveTimeout();

	FAIRequestID ActiveMoveId = FAIRequestID::InvalidRequest;
	FDelegateHandle MoveFinishedHandle;
	FTimerHandle MoveTimeoutHandle;

	static constexpr float AcceptanceRadius = 20.0f;
	static constexpr float MoveTimeoutSeconds = 200.0f;
};