#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "TimerManager.h"
#include "HotelPhoneActor.generated.h"

class UCameraComponent;
class UAudioComponent;
class USoundBase;
class UPhoneSubtitleWidget;

// LightAnomaly 등 C++ 클래스에서 구독할 알림입니다.
DECLARE_MULTICAST_DELEGATE_OneParam(
	FHotelPhoneCallEvent,
	const UObject*);

// 전화 요청 하나의 정보.
// 요청자를 강하게 참조하지 않으므로 제거된 액터를 붙잡지 않습니다.
struct FHotelPhoneCallRequest
{
	TWeakObjectPtr<const UObject> Requester;
	FText Subtitle;
	bool bHasRequester = false;
};

UCLASS()
class THELASTCHECKIN_API AHotelPhoneActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHotelPhoneActor();

	// 기존 BP 호출 유지: 요청자 없는 전화를 대기열에 추가합니다.
	UFUNCTION(BlueprintCallable, Category = "Phone")
	void StartRinging(const FText& InSubtitle);

	// 현재 울리는 전화만 취소합니다. 진행 중인 통화는 끊지 않습니다.
	UFUNCTION(BlueprintCallable, Category = "Phone")
	void StopRinging();

	UFUNCTION(BlueprintPure, Category = "Phone")
	bool IsRinging() const
	{
		return bRinging;
	}

	UFUNCTION(BlueprintPure, Category = "Phone")
	bool IsInCall() const
	{
		return bInCall;
	}

	UFUNCTION(BlueprintPure, Category = "Phone")
	bool IsBusy() const
	{
		return bRinging || bInCall || bFinishingCall;
	}

	// 기존 AnomalyBase와 LightAnomaly의 호출 유지
	void StartRingingFor(
		const UObject* InRequester,
		const FText& InSubtitle);

	void StopRingingFor(const UObject* InRequester);

	// 새 코드에서는 이 함수를 사용하면 접수 결과를 확인할 수 있습니다.
	// true: 대기열 접수 또는 기존 미수신 요청의 대사 갱신
	// false: 잘못된 요청자, 종료 중, 같은 요청자가 이미 통화 중
	bool RequestCallFor(
		const UObject* InRequester,
		const FText& InSubtitle);

	// 해당 요청자의 전화가 대기·벨소리·통화 상태인지 확인합니다.
	bool HasCallFor(const UObject* InRequester) const;

	// C++ 이벤트. 요청자 없는 전화는 nullptr를 전달합니다.
	FHotelPhoneCallEvent OnCallAnswered;
	FHotelPhoneCallEvent OnCallFinished;

	virtual bool CanInteract_Implementation(
		AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(
		AActor* Interactor) const override;

	virtual void Interact_Implementation(
		AActor* Interactor) override;

protected:
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phone")
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Phone")
	TObjectPtr<UAudioComponent> RingAudio;

	UPROPERTY(EditAnywhere, Category = "Phone")
	TObjectPtr<USoundBase> RingSound;

	UPROPERTY(EditAnywhere, Category = "Phone")
	FText AnswerPrompt;

	UPROPERTY(EditAnywhere, Category = "Phone",
		meta = (ClampMin = "0.1", Units = "s"))
	float CallDuration = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Phone",
		meta = (ClampMin = "0.1", Units = "s"))
	float CameraBlendTime = 0.5f;

private:
	void TryStartNextCall();
	void StopCurrentRing();
	void EndCall();

	TArray<FHotelPhoneCallRequest> PendingCalls;
	FHotelPhoneCallRequest CurrentCall;

	TWeakObjectPtr<APlayerController> CallerPC;
	FTimerHandle CallTimer;

	UPROPERTY(Transient)
	TObjectPtr<UPhoneSubtitleWidget> SubtitleWidget;

	bool bRinging = false;
	bool bInCall = false;
	bool bFinishingCall = false;
	bool bEndingPlay = false;
};