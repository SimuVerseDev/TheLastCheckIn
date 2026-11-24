#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "Engine/TimerHandle.h"
#include "FallObjectActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class ATrashBagActor;

UENUM(BlueprintType)
enum class EFallObjectState : uint8
{
	Intact,  // 제자리에 멀쩡히 있음
	Broken,  // 떨어져서 잔해 상태 - E 홀드로 봉투에 담기
	Bagged,  // 잔해를 봉투에 담음 - 봉투를 쓰레기통에 버려야 제거 완료
	Empty    // 제거 완료, 빈 자리 - 상점에서 사서 설치하면 Intact로 복구 (거울은 다음 날 저절로 복구)
};

/*
낙하 이상현상 대상 (스탠드 조명, 화분, 벽걸이 거울, 액자)
BP에서 IntactMesh / BrokenMesh에 메시를 넣고, BrokenMesh는 떨어진 바닥 위치에 미리 배치
AFallAnomaly가 레벨에서 자동으로 모아서 떨어뜨림
*/
UCLASS()
class THELASTCHECKIN_API AFallObjectActor
	: public AActor
	, public IInteractable
{
	GENERATED_BODY()

public:
	AFallObjectActor();

	UFUNCTION(BlueprintCallable, Category="Night|Fall")
	bool Fall();

	// 테스트용 - PIE 중 디테일 패널 버튼으로 바로 떨어뜨림
	UFUNCTION(CallInEditor, Category="Night|Fall|Debug")
	void DebugFall() { Fall(); }

	// 상점 재배치로 원래 상태로 복구
	UFUNCTION(BlueprintCallable, Category="Night|Fall")
	bool Restore();

	/*
	제자리에서 깨지는 것(bFallOnBreak = false, 벽걸이 거울)만 - 치웠는지와 상관없이 원래 상태로 복구
	다음 날 아침 AFallAnomaly가 호출, 교체가 아니라 평판 보너스 없음
	*/
	UFUNCTION(BlueprintCallable, Category="Night|Fall")
	bool RestoreForNewDay();

	UFUNCTION(BlueprintPure, Category="Night|Fall")
	EFallObjectState GetFallState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Night|Fall")
	FName GetRefillProductId() const { return RefillProductId; }

	// 빈 자리 표시 - 같은 상품을 설치·재배치·운반 중일 때 AFallAnomaly가 켬 (Empty일 때만 보임)
	void SetSlotHintVisible(bool bVisible);

	// 오늘 밤 떨어졌고 아직 봉투에 담지 않았는지 - 정산 때 평판 판정 후 AFallAnomaly가 초기화
	bool HasFallenTonight() const { return bFellTonight; }
	void ClearFallenTonight() { bFellTonight = false; }

	/*
	IInteractable
	잔해를 담은 봉투를 들고 있는 동안에는 봉투로 그대로 전달 (봉투가 진행 중인 상호작용이 되도록)
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual float GetInteractHoldDuration_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;

	/*
	LMB - 멀쩡한 오브젝트나 잔해를 BP_HK_TableLamp처럼 들기 (HKCarryComponent 사용)
	*/
	virtual bool CanUse_Implementation(AActor* Interactor) const override;
	virtual FText GetUsePrompt_Implementation(AActor* Interactor) const override;
	virtual bool Use_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Fall")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Fall")
	TObjectPtr<UStaticMeshComponent> IntactMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Fall")
	TObjectPtr<UStaticMeshComponent> BrokenMesh;

	// 빈 자리에 띄우는 원래 오브젝트 모양 - 충돌 없음, IntactMesh의 메시를 SlotHintMaterial로 덮음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Fall")
	TObjectPtr<UStaticMeshComponent> SlotHintMesh;

	// 빈 자리 표시 머티리얼 - 기본은 하우스키핑 쓰레기 하이라이트(M_HK_TrashHighlight)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall")
	TObjectPtr<UMaterialInterface> SlotHintMaterial;

	// 다시 채울 때 쓰는 상점 상품 ID (상품 DataTable의 행 이름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall")
	FName RefillProductId;

	/*
	물리
	true: 멀쩡한 동안 LMB로 들 수 있는 오브젝트 (스탠드 조명, 화분) - BP_HK_TableLamp와 같은 방식
	      평소엔 물리 꺼진 채 고정, 잡는 순간 물리 켬, 원래 자리 근처에 내려놓으면 제자리 스냅
	false: 떨어질 때만 물리를 켬 - 벽에 걸린 것 (벽걸이 거울, 액자), 액터 정면(X+)이 벽 반대쪽을 향하게 배치
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Physics")
	bool bSimulateWhileIntact = true;

	/*
	들기 - BP_HK_TableLamp(AHKPhysicsPickupActor + HKPlacementDirtComponent)와 같은 값
	*/
	// 들고 있는 동안 기울기 없이 똑바로 세움
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Carry")
	bool bUprightWhileCarried = true;

	// 내려놓았을 때 원래 자리와 이 수평 거리 안이면 제자리 스냅
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Carry", meta=(ClampMin="0", Units="cm"))
	float SnapDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Carry", meta=(ClampMin="0", Units="cm"))
	float SnapHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Carry", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float SnapAngle = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Carry", meta=(ClampMin="0.01", Units="s"))
	float SnapDuration = 0.2f;

	/*
	true: 이상현상 때 잔해가 물리로 떨어져 쓰러짐
	false: 떨어지지 않고 제자리에서 깨짐 (벽걸이 거울 등) - BrokenMesh가 BP에 배치한 자리에 그대로 나타나고 바로 치우기 가능
	       치우든 안 치우든 다음 날 아침 원래 상태로 돌아옴 (상점 교체 없음)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Physics")
	bool bFallOnBreak = true;

	// 멀쩡한 오브젝트 무게 - Carry의 MaxCarryMass(기본 50kg)보다 크면 들 수 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Physics", meta=(ClampMin="0.1", Units="kg"))
	float IntactMassKg = 10.0f;

	// 이상현상으로 떨어질 때 윗부분을 미는 속도 - 무게와 상관없이 같은 정도로 쓰러짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Physics", meta=(ClampMin="0", Units="cm/s"))
	float FallImpulse = 250.0f;

	// LMB로 들 수 있는지 - 멀쩡할 때(물리가 켜져 있을 때)와 잔해일 때(쓰러지는 중 제외)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Physics")
	bool bCanBePickedUp = true;

	UPROPERTY(EditAnywhere, Category="Night|Fall|Physics")
	FText PickupPrompt;

	/*
	떨어지면 즉시 잔해로 바뀌어 물리로 쓰러짐 -> 이 시간 뒤 멈춘 자리에 고정 (그 뒤부터 치우기 가능)
	잔해가 떨어지기 시작하는 위치는 BP의 BrokenMesh 위치 - 벽에 걸린 것은 벽 높이에 배치
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall|Physics", meta=(ClampMin="0", Units="s"))
	float SettleTime = 2.0f;

	// 잔해 치우기 E 홀드 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall", meta=(ClampMin="0.1", Units="s"))
	float CleanupHoldSeconds = 0.75f;

	// 잔해를 치우면 손에 쥐여 줄 봉투
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night|Fall")
	TSubclassOf<ATrashBagActor> BagClass;

	UPROPERTY(EditAnywhere, Category="Night|Fall")
	FText CleanupPrompt;

	/*
	연출
	*/
	// 떨어지기 시작
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Fall")
	void OnFell();

	// 잔해가 멈춰서 고정됨
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Fall")
	void OnBroken();

	UFUNCTION(BlueprintImplementableEvent, Category="Night|Fall")
	void OnCleanedUp();

	UFUNCTION(BlueprintImplementableEvent, Category="Night|Fall")
	void OnCleared();

	UFUNCTION(BlueprintImplementableEvent, Category="Night|Fall")
	void OnRestored();

private:
	UFUNCTION()
	void HandleBagDisposed(ATrashBagActor* DisposedBag);

	// 플레이어가 들고 있던 메시를 놓음 - 던지지 않았으면 제자리 스냅 시도
	UFUNCTION()
	void HandleCarryReleased(UPrimitiveComponent* Component, bool bThrown);

	bool TrySnapIntactHome();
	void TickIntactSnap();
	void StopIntactSnap();

	ATrashBagActor* GetBagHeldBy(const AActor* Interactor) const;
	void ApplyState();
	void RestoreToIntact();
	void Settle();
	void ResetIntactMesh();
	void ResetBrokenMesh();
	UStaticMeshComponent* GetCarriableMesh() const;
	bool CanBeCarried() const;
	void ReleaseFromCarrier();

	EFallObjectState State = EFallObjectState::Intact;
	bool bFellTonight = false;

	// AFallAnomaly가 표시를 요청했는지 - 실제로는 Empty일 때만 보임
	bool bSlotHintRequested = false;

	// 잔해가 물리로 쓰러지는 중 (Broken이지만 아직 고정 전)
	bool bFalling = false;

	FTimerHandle SettleTimer;

	// 멀쩡한 오브젝트 제자리 스냅 (내려놓았을 때)
	FTimerHandle IntactSnapTimer;
	FTransform IntactSnapStart;
	float IntactSnapStartTime = 0.0f;

	// 들고 있는 플레이어의 Carry - 놓을 때 알림 받기용
	TWeakObjectPtr<class UHKCarryComponent> BoundCarry;

	// 배치된 원래 위치 (Root 기준)
	FTransform IntactHome;
	FTransform BrokenHome;

	TWeakObjectPtr<ATrashBagActor> Bag;
};
