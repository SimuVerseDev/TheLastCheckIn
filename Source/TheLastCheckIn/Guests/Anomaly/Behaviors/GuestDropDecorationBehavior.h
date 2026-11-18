#pragma once

#include "CoreMinimal.h"
#include "Guests/Anomaly/Behaviors/GuestMoveToPointBehavior.h"
#include "TimerManager.h"
#include "GuestDropDecorationBehavior.generated.h"

class AThrowableDecoration;

UCLASS()
class THELASTCHECKIN_API UGuestDropDecorationBehavior
	: public UGuestMoveToPointBehavior
{
	GENERATED_BODY()

public:
	virtual bool CanStart() const override;

	UFUNCTION(BlueprintCallable, Category = "Anomaly|Decoration")
	void HandleKnockDown();

	UFUNCTION(BlueprintCallable, Category = "Anomaly|Decoration")
	void HandlePreparePickup();

	UFUNCTION(BlueprintCallable, Category = "Anomaly|Decoration")
	void HandleGrab();

	UFUNCTION(BlueprintCallable, Category = "Anomaly|Decoration")
	void HandleThrow();

protected:
	virtual void OnStarted_Implementation() override;
	virtual void OnStopped_Implementation() override;

	// 공통 이동 부모가 이동 완료 후 호출
	virtual void BeginActing() override;

private:
	enum class EPropStage : uint8
	{
		WaitingForKnock,
		OnGround,
		ReadyToGrab,
		Holding
	};

	bool CanHandlePropEvent() const;
	void CancelPropAction(const TCHAR* Reason);
	void PerformAction();

	UPROPERTY(Transient)
	TObjectPtr<AThrowableDecoration> TargetProp;

	EPropStage PropStage = EPropStage::WaitingForKnock;
	bool bThrowMontageStarted = false;

	// 장식물 집기에만 필요한 위치
	FTransform PickupTransform = FTransform::Identity;

	FTimerHandle ActionTimerHandle;
};