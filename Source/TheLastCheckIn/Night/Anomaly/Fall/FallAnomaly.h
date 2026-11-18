#pragma once

#include "CoreMinimal.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "FallAnomaly.generated.h"

class AFallObjectActor;

/*
낙하 이상현상 - 로비, 복도의 오브젝트가 밤 동안 하나씩 떨어져 부서짐

상태 전이를 하지 않음 (항상 Idle)
-> 매니저의 해결 알림, 정산 해결/미해결 표시, 방치 악화, 밤 마감 실패 처리 대상이 아님
-> 평판은 이 클래스가 직접 처리

- 22시에 매니저가 StartAnomaly 호출 -> 멀쩡한 오브젝트 중 MinFallCount~MaxFallCount개를 골라 FallUntilHour 전까지 각자 무작위 시각에 떨어뜨림
- 정산 시작 직전(PenaltyLeadMinutes 전) 오늘 밤 떨어졌는데 봉투에 담지 않은 것마다 Complaint, 다음 날부터는 방치해도 감소 없음
- 상점에서 RefillProductId 상품을 설치하면 가장 가까운 빈 자리로 복구하고 CleanRoomAssigned
*/
UCLASS()
class THELASTCHECKIN_API AFallAnomaly : public AAnomalyBase
{
	GENERATED_BODY()

public:
	AFallAnomaly();

	virtual bool StartAnomaly() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category="Night|Fall", meta=(ClampMin="0"))
	int32 MinFallCount = 1;

	UPROPERTY(EditAnywhere, Category="Night|Fall", meta=(ClampMin="0"))
	int32 MaxFallCount = 3;

	// 이 시각 전까지 떨어짐 (다음 날 새벽)
	UPROPERTY(EditAnywhere, Category="Night|Fall", meta=(ClampMin="0", ClampMax="23"))
	int32 FallUntilHour = 4;

	// 정산 시작 몇 분 전에 평판 판정할지 (정산 화면에 감점이 잡히도록 정산 확정 전에 처리)
	UPROPERTY(EditAnywhere, Category="Night|Fall", meta=(ClampMin="1", Units="min"))
	int32 PenaltyLeadMinutes = 1;

private:
	UFUNCTION()
	void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

	UFUNCTION()
	void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);

	UFUNCTION()
	void HandlePurchaseConfirmed(FName ProductId, AActor* Product);

	void CollectFallObjects();
	void ApplyNightPenalty();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFallObjectActor>> FallObjects;

	// 아직 떨어지지 않은 오브젝트와 떨어질 게임 시각(초)
	UPROPERTY(Transient)
	TMap<TObjectPtr<AFallObjectActor>, double> PendingFalls;
};
