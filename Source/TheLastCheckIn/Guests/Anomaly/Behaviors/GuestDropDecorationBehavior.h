#pragma once

#include "CoreMinimal.h"
#include "Guests/Anomaly/GuestAnomalyBehavior.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "GuestDropDecorationBehavior.generated.h"

class AGuestAIController;
class AThrowableDecoration;

UCLASS()
class THELASTCHECKIN_API UGuestDropDecorationBehavior
	: public UGuestAnomalyBehavior
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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//목적지로 이동
	virtual void OnStarted_Implementation() override;
	//이동, 타이머, 애니메이션 정리
	virtual void OnStopped_Implementation() override;
	
	virtual void OnSignalReceived_Implementation(
	FName Signal,
	AActor* InstigatorActor) override;

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

    UPROPERTY(Transient)
    TObjectPtr<AThrowableDecoration> TargetProp;

    EPropStage PropStage = EPropStage::WaitingForKnock;
    bool bThrowMontageStarted = false;

    // 물건이 날아가도 이동/집기/던지기의 기준은 따라 움직이지 않는다.
    FTransform ActionTransform = FTransform::Identity;
    FTransform PickupTransform = FTransform::Identity;

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
	static constexpr float AcceptanceRadius = 5.0f;
};
