#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "StoryRadio.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class UBoxComponent;
class UCameraComponent;
class USoundBase;
class UPhoneSubtitleWidget;

UENUM(BlueprintType)
enum class EStoryRadioState : uint8
{
	Off,
	Waiting,
	Static,     // 잡음 재생 중 (E로 들을 수 있음)
	Listening,
	Done
};

USTRUCT(BlueprintType)
struct FStoryRadioSubtitleLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", Units = "s"))
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, meta = (MultiLine = "true"))
	FText Text;
};

UCLASS()
class THELASTCHECKIN_API AStoryRadio : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AStoryRadio();
	
	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

	void SetState(EStoryRadioState NewState);

	double WindowStartSeconds = 0.0;
	double WindowEndSeconds = 0.0;
	
	// ---------- 컴포넌트 ----------
	UPROPERTY(VisibleAnywhere, Category = "Radio")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Radio")
	TObjectPtr<UAudioComponent> RadioAudio;

	UPROPERTY(VisibleAnywhere, Category = "Radio")
	TObjectPtr<UBoxComponent> CounterZone;

	UPROPERTY(VisibleAnywhere, Category = "Radio")
	TObjectPtr<UCameraComponent> ViewCamera;

	// ---------- 일정 ----------
	UPROPERTY(EditAnywhere, Category = "Radio|Schedule", meta = (ClampMin = "1"))
	int32 BroadcastDay = 2;

	UPROPERTY(EditAnywhere, Category = "Radio|Schedule", meta = (ClampMin = "0", ClampMax = "23"))
	int32 StartHour = 20;
	
	UPROPERTY(EditAnywhere, Category = "Radio|Schedule", meta = (ClampMin = "0.1"))
	float WindowHours = 8.0f; // 20시 + 8시간 = 다음날 04시
	
	UPROPERTY(EditAnywhere, Category = "Radio|Schedule", meta = (ClampMin = "0.1"))
	float StaticHours = 2.0f;

	// ---------- 사운드 ----------
	UPROPERTY(EditAnywhere, Category = "Radio|Sound")
	TObjectPtr<USoundBase> StaticSound;

	UPROPERTY(EditAnywhere, Category = "Radio|Sound")
	TObjectPtr<USoundBase> TuningSound;

	UPROPERTY(EditAnywhere, Category = "Radio|Sound")
	TObjectPtr<USoundBase> NewsSound;

	UPROPERTY(EditAnywhere, Category = "Radio|View", meta = (ClampMin = "0.1", Units = "s"))
	float CameraBlendTime = 0.5f;

	// ---------- 상태 ----------
	UPROPERTY(VisibleInstanceOnly, Category = "Radio|Debug")
	EStoryRadioState State = EStoryRadioState::Off;
	
	UFUNCTION()
	void HandleCounterBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	
	bool IsPlayerActor(const AActor* Actor) const;
	bool IsPlayerInCounter() const;
	
	void StartStatic();
	void StopRadioAudio(float FadeOutSeconds);
	
	double GetNowSeconds() const;
	
	/*
	잡음이 꺼지는 시각
	*/
	double StaticEndSeconds = 0.0;
	
	UPROPERTY(EditAnywhere, Category = "Radio|Sound", meta = (ClampMin = "0", Units = "s"))
	float StaticFadeInSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Radio|Sound", meta = (ClampMin = "0", Units = "s"))
	float StaticFadeOutSeconds = 1.5f;
	
	UPROPERTY(EditAnywhere, Category = "Radio|Sound", meta = (ClampMin = "0", Units = "s"))
	float NewsTailSeconds = 0.8f;
	
	UPROPERTY(EditAnywhere, Category = "Radio|View")
	FText ListenPrompt = NSLOCTEXT("StoryRadio", "Listen", "라디오 듣기");

	void StartListening(APlayerController* PC);
	void PlayNews();
	void FinishListening();

	TWeakObjectPtr<APlayerController> ListenerPC;
	FTimerHandle ListenTimer;

	/*
	자막
	*/
	UPROPERTY(EditAnywhere, Category = "Radio|Subtitle")
	TArray<FStoryRadioSubtitleLine> NewsSubtitles;

	void ShowNextSubtitle();
	void HideSubtitle();

	UPROPERTY(Transient)
	TObjectPtr<UPhoneSubtitleWidget> SubtitleWidget;
	FTimerHandle SubtitleTimer;
	int32 SubtitleIndex = 0;
};