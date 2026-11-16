#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "BellActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class UBoxComponent;
class USoundBase;
class ABellActor;

UENUM(BlueprintType)
enum class EBellState : uint8
{
	Idle,     // 평소 - 걸려 있기만 함
	Ringing,  // 이상현상으로 울리는 중
	Broken,   // 부서진 채 남아 있음
	Removed   // 치워져서 빈 자리 - 상점에서 다시 사야 함
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBellEvent, ABellActor*, Bell);

/*
로비 종 판(BP_RoomBellSet)에 걸린 객실 호출 종 하나 - RoomNumber 객실과 연결
울리는 중 E: 부수기 / 부서진 뒤 E: 치우기
*/
UCLASS()
class THELASTCHECKIN_API ABellActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ABellActor();

	UFUNCTION(BlueprintCallable, Category="Night|Bell")
	void StartRinging();

	UFUNCTION(BlueprintCallable, Category="Night|Bell")
	void StopRinging();

	/*
	치운 자리에 새 종을 건다 - 추후 상점 구매에서 호출
	*/
	UFUNCTION(BlueprintCallable, Category="Night|Bell")
	void RestoreBell();

	UFUNCTION(BlueprintPure, Category="Night|Bell")
	EBellState GetBellState() const
	{
		return BellState;
	}

	UFUNCTION(BlueprintPure, Category="Night|Bell")
	int32 GetRoomNumber() const
	{
		return RoomNumber;
	}

	// 방 번호 앞자리 = 층 (203 → 2층)
	UFUNCTION(BlueprintPure, Category="Night|Bell")
	int32 GetFloor() const
	{
		return RoomNumber / 100;
	}

	UPROPERTY(BlueprintAssignable, Category="Night|Bell")
	FBellEvent OnBroken;

	UPROPERTY(BlueprintAssignable, Category="Night|Bell")
	FBellEvent OnRemoved;

	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 연출용 BP 훅
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Bell")
	void OnRingStarted();
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Bell")
	void OnBellBroken();
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Bell")
	void OnBellRemoved();
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Bell")
	void OnBellRestored();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<USceneComponent> Root;

	/*
	Root
	├ Hanger      종 걸이 (bell01) - 고정, 치워도 남음
	├ NameTag     이름표 - 고정, 치워도 남음, RoomNumber를 Custom Primitive Data[0]로 표시
	└ SwingPivot  매달린 지점 - 울릴 때 이것이 회전
	  ├ Mesh      종 몸체 (bell03)
	  ├ Clapper   추 (bell02)
	  └ InteractBox  E 상호작용 조준 영역
	치울 때 SwingPivot 아래 전부 숨김 - 이름표 등 남길 컴포넌트는 Root에 붙일 것
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<UStaticMeshComponent> Hanger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<USceneComponent> SwingPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<UStaticMeshComponent> Clapper;

	// 상호작용 시선 트레이스용 - 메시 콜리전 유무와 상관없이 종 몸체를 조준하면 잡힘
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<UBoxComponent> InteractBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<UAudioComponent> RingAudio;

	/*
	이름표 (bell_nameTag) - Root에 붙어 있어 종을 치워도 남음
	M_NameTagDigits가 Custom Primitive Data[0] 값을 숫자로 그림 - RoomNumber가 자동으로 들어감
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell")
	TObjectPtr<UStaticMeshComponent> NameTag;

	// 연결된 객실 번호 - 이름표에 표시됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Bell")
	int32 RoomNumber = 0;

	/*
	종을 좌우 반전 (판 왼쪽 절반처럼 몸체가 반대쪽에 매달린 종)
	걸이와 흔들리는 부품만 반전하고, 이름표는 위치만 옮겨 숫자가 뒤집히지 않음
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Bell")
	bool bMirrored = false;

	// 비워 두면 RingAudio 컴포넌트에 지정된 사운드 사용 - 울리는 동안 끝날 때마다 다시 재생
	UPROPERTY(EditAnywhere, Category="Night|Bell|Sound")
	TObjectPtr<USoundBase> RingSound;

	// 재생 속도 배율 (1 = 원래 속도) - 빠를수록 음도 높아짐
	UPROPERTY(EditAnywhere, Category="Night|Bell|Sound", meta=(ClampMin="0.1", ClampMax="4"))
	float RingPlaybackSpeed = 1.0f;

	/*
	0보다 크면 이 간격마다 새 소리를 따로 재생해 앞 소리 여운 위에 겹침 (RingRepeatDelay 무시)
	0이면 RingAudio 하나로 끝날 때마다 다시 재생
	*/
	UPROPERTY(EditAnywhere, Category="Night|Bell|Sound", meta=(ClampMin="0", Units="s"))
	float RingInterval = 0.0f;

	// 겹침 모드에서 부수거나 멈출 때 남은 소리가 사라지는 시간
	UPROPERTY(EditAnywhere, Category="Night|Bell|Sound", meta=(ClampMin="0", Units="s", EditCondition="RingInterval > 0"))
	float RingStopFadeTime = 0.15f;

	// 한 번 울린 뒤 다시 울리기까지 쉬는 시간 (0이면 바로 이어서) - RingInterval이 0일 때만 사용
	UPROPERTY(EditAnywhere, Category="Night|Bell|Sound", meta=(ClampMin="0", Units="s", EditCondition="RingInterval <= 0"))
	float RingRepeatDelay = 0.0f;

	UPROPERTY(EditAnywhere, Category="Night|Bell|Sound")
	TObjectPtr<USoundBase> BreakSound;

	// 비어 있으면 메시는 그대로 두고 상태만 바뀜
	UPROPERTY(EditAnywhere, Category="Night|Bell")
	TObjectPtr<UStaticMesh> BrokenMesh;

	// 울리는 동안 좌우로 흔들리는 최대 각도
	UPROPERTY(EditAnywhere, Category="Night|Bell|Ring", meta=(ClampMin="0", Units="deg"))
	float SwingAngle = 12.0f;

	// 초당 흔들림 횟수
	UPROPERTY(EditAnywhere, Category="Night|Bell|Ring", meta=(ClampMin="0.1"))
	float SwingFrequency = 4.0f;

	UPROPERTY(EditAnywhere, Category="Night|Bell")
	FText BreakPrompt;

	UPROPERTY(EditAnywhere, Category="Night|Bell")
	FText RemovePrompt;

private:
	void Break();
	void Remove();
	void SetSwingPartsPresent(bool bPresent);
	void UpdateNameTag();
	void ApplyMirror();
	void PlayRingSound();
	void PlayOverlapRing();
	void StopRingSound();

	UFUNCTION()
	void HandleRingSoundFinished();

	FTimerHandle RingRepeatTimer;

	// 겹침 모드로 재생 중인 소리들 - 끝나면 자동 파괴
	TArray<TWeakObjectPtr<UAudioComponent>> OverlapVoices;

	EBellState BellState = EBellState::Idle;

	// RestoreBell 때 되돌릴 원래 메시와 자세 (에셋이라 GC 대상 아님)
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> OriginalMesh;
	FRotator PivotHomeRotation = FRotator::ZeroRotator;

	// 피벗 아래 부품들의 원래 콜리전
	TMap<TWeakObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionEnabled::Type>> SwingPartCollisions;
	float RingElapsed = 0.0f;
};
