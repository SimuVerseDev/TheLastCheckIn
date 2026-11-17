#pragma once

#include "CoreMinimal.h"
#include "Guests/Anomaly/Behaviors/GuestMoveToPointBehavior.h"
#include "NavigationData.h"
#include "GuestNoiseBehavior.generated.h"

class AGuestActionPoint;

UCLASS()
class THELASTCHECKIN_API UGuestNoiseBehavior
	: public UGuestMoveToPointBehavior
{
	GENERATED_BODY()

public:
	virtual bool CanStart() const override;

protected:
	virtual void OnStarted_Implementation() override;
	virtual void OnStopped_Implementation() override;
	virtual void BeginActing() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AGuestActionPoint> Point;
	
	// 구역 안 이동을 시작하고 주기적으로 진행 상태를 검사한다.
	void StartRoaming();
	void UpdateRoaming();

	// 구역 안 목적지와 경로를 찾아 이동을 요청한다.
	bool TryStartRoamMove();
	bool IsPathInsideZone(const FNavPathSharedPtr& Path) const;

	FTimerHandle RoamTimer;

	double NextRoamSeconds = 0.0;
	double RoamMoveStartSeconds = -1.0;

	// 행동 종료 시 원래 이동 설정으로 복원하기 위한 값.
	bool bSavedMovement = false;
	float OriginalWalkSpeed = 0.0f;
	// 소음 행동 종료 시 복원할 회전 속도
	FRotator OriginalRotationRate = FRotator::ZeroRotator;
	bool bOriginalOrientToMovement = false;
	bool bOriginalUseControllerDesiredRotation = false;
	bool bOriginalUseControllerRotationYaw = false;
};
