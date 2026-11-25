#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "TimerManager.h"
#include "DayOneAnomalySubsystem.generated.h"

class UGameClockSubsystem;
class UHotelPhaseSubsystem;
class AActor;
class ULightComponent;
class ACCTVMonitor;
class AShadowFigureActor;
class AKnockDoorActor;
class UAudioComponent;

UENUM(BlueprintType)
enum class EDayOneAnomaly : uint8
{
	LobbyFlicker,
	ShadowFigure,
	DoorKnock
};

// Finished means the presentation ended, not that the player solved an anomaly.
UENUM(BlueprintType)
enum class EDayOneAnomalyState : uint8
{
	Waiting,
	Active,
	Finished,
	Skipped
};

USTRUCT(BlueprintType)
struct FDayOneAnomalyProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EDayOneAnomalyState State = EDayOneAnomalyState::Waiting;
	UPROPERTY(BlueprintReadOnly) double WindowStartSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly) double WindowEndSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly) double StartedAtSeconds = -1.0;
	UPROPERTY(BlueprintReadOnly) double EndAtSeconds = -1.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDayOnePresentationEvent, EDayOneAnomaly, Presentation);

// Scheduling foundation only. Visual/audio adapters subscribe to these events later.
// Never registers with AnomalyManager: no escalation, complaints, settlement or memo logic.
UCLASS(BlueprintType)
class THELASTCHECKIN_API UDayOneAnomalySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Tutorial|Day One")
	bool IsScheduleActive() const { return bScheduleActive; }

	UFUNCTION(BlueprintPure, Category="Tutorial|Day One")
	FDayOneAnomalyProgress GetProgress(EDayOneAnomaly Presentation) const;

	UFUNCTION(BlueprintPure, Category="Tutorial|Day One")
	double GetPlannedFlickerStartSeconds() const { return PlannedFlickerStartSeconds; }

	// Future CCTV / staff-room adapters report their current context here.
	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One")
	void SetCCTVViewing(bool bViewing);

	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One")
	void SetPlayerInStaffRoom(bool bInside);

	// For a presentation adapter to report an early exit (e.g. opening the knocked door).
	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One")
	void EndPresentation(EDayOneAnomaly Presentation);

	UPROPERTY(BlueprintAssignable, Category="Tutorial|Day One")
	FDayOnePresentationEvent OnPresentationStarted;

	// Also emitted for active presentations when the day/phase ends or the world shuts down.
	UPROPERTY(BlueprintAssignable, Category="Tutorial|Day One")
	FDayOnePresentationEvent OnPresentationEnded;
	
	// 레벨에서 지정한 조명 액터들.
	// 각 액터에 직접 포함된 Light 컴포넌트를 모두 대상으로 사용합니다.
	// 진행 중에는 대상을 교체하지 않습니다.
	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One|Lobby")
	void SetLobbyLightActors(const TArray<AActor*>& Actors);

	// 로비 트리거에서 플레이어 진입/퇴장 시 호출합니다.
	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One|Lobby")
	void SetPlayerInLobby(bool bInside);
	
	// 기존 CCTV 모니터와 첫날용 형체 인스턴스를 지정합니다.
	// 형체는 에디터에서 CCTV Only를 켜고,
	// 기존 ShadowAnomaly의 대상 목록에는 넣지 않습니다.
	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One|Shadow")
	void SetShadowPresentationTargets(
		ACCTVMonitor* Monitor,
		AShadowFigureActor* LobbyFigure,
		AShadowFigureActor* SecondFloorFigure);
	
	// 기존 노크 사운드가 설정된 BP 클래스와 CCTV 모니터를 지정합니다.
	UFUNCTION(BlueprintCallable, Category="Tutorial|Day One|Knock")
	void ConfigureDoorKnock(
		TSubclassOf<AKnockDoorActor> InKnockClass,
		ACCTVMonitor* Monitor);

private:
	UFUNCTION() void HandleDayStarted(int32 Day);
	UFUNCTION() void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);
	UFUNCTION() void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);
	void Evaluate(double Now);
	void StartPresentation(EDayOneAnomaly Presentation, double Now, double EndAt);
	void StopSchedule();

	UPROPERTY(Transient) TObjectPtr<UGameClockSubsystem> Clock;
	UPROPERTY(Transient) TObjectPtr<UHotelPhaseSubsystem> Phase;
	UPROPERTY(Transient) TMap<EDayOneAnomaly, FDayOneAnomalyProgress> Progress;
	bool bScheduleActive = false;
	bool bDayOneScheduled = false;
	bool bViewingCCTV = false;
	bool bInStaffRoom = false;
	double PlannedFlickerStartSeconds = -1.0;
	
	bool StartLobbyFlicker();
	void ToggleLobbyFlicker();
	void StopLobbyFlicker();

	bool bInLobby = false;
	bool bLobbyFlickering = false;
	bool bLobbyLightOutputOn = true;

	// 실제 깜빡임 간격: 월드 시간 기준 초.
	// 연출 전체 지속 시간인 '게임 시간 1시간'과는 별개입니다.
	float LobbyMinFlickerInterval = 0.08f;
	float LobbyMaxFlickerInterval = 0.35f;

	FTimerHandle LobbyFlickerTimer;

	TArray<TWeakObjectPtr<ULightComponent>> LobbyLights;

	// 시작 당시의 켜짐/꺼짐 상태. 종료 시 이 상태로 복원합니다.
	TMap<TWeakObjectPtr<ULightComponent>, bool> LobbyOriginalVisibility;
	
	// 기존 CCTV 클래스의 공개 함수를 읽어 시청 상태를 갱신합니다.
	void PollCCTVViewing();

	bool StartShadowPresentation();
	void UpdateShadowVisibility();
	void StopShadowPresentation();

	TWeakObjectPtr<ACCTVMonitor> ShadowCCTVMonitor;

	TArray<TWeakObjectPtr<AShadowFigureActor>> ShadowCandidates;
	TWeakObjectPtr<AShadowFigureActor> ActiveShadowFigure;

	FTimerHandle CCTVViewingTimer;
	
	bool StartDoorKnock();
	void TickDoorKnock();
	void FinishDoorKnock();
	void StopDoorKnock();

	UPROPERTY(Transient)
	TSubclassOf<AKnockDoorActor> DoorKnockClass;

	TWeakObjectPtr<AKnockDoorActor> ActiveKnockActor;
	TWeakObjectPtr<UAudioComponent> ActiveKnockAudio;

	FTimerHandle DoorKnockTickTimer;
	FTimerHandle DoorKnockEndTimer;

	// 실제 월드 시간 기준 초
	float DoorKnockDurationSeconds = 10.0f;
	float DoorKnockRepeatInterval = 2.5f;
	double NextDoorKnockSeconds = 0.0;

	// 플레이어 주변에서 처음 선택한 실제 발생 위치
	FVector DoorKnockWorldLocation = FVector::ZeroVector;
	
	void ReportPresentationWitnessed(EDayOneAnomaly Presentation);
	void CheckShadowWitness();

	TSet<EDayOneAnomaly> WitnessedPresentations;

	// CCTV에서 형체가 연속해서 보이기 시작한 월드 시간
	double ShadowVisibleSinceSeconds = -1.0;
	
};
