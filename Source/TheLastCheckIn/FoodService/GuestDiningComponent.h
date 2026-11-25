#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GuestDiningComponent.generated.h"

class ABuffetStation;

// 손님 한 명의 식당 줄서기, 음식 자리 이용, 객실 복귀를 관리한다.
UCLASS(ClassGroup = (Guests),
	meta = (BlueprintSpawnableComponent))
class THELASTCHECKIN_API UGuestDiningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGuestDiningComponent();

	// 매니저가 조식 시간에 호출
	bool TryStartBreakfast(
		const TArray<TObjectPtr<ABuffetStation>>& Stations,
		double CurrentSeconds);

	// 영업 종료 후에도 복귀 처리를 위해 계속 호출
	void AdvanceBreakfast(
		double CurrentSeconds,
		bool bBreakfastOpen);

	// 손님 풀 반환 시 예약과 방문 기록을 초기화
	void ResetForPool();

	UFUNCTION(BlueprintPure, Category = "Guests|Dining")
	bool IsBusy() const
	{
		return bVisitActive;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 식사 기능 연결 전, 진열대에서 대기할 게임 시간(분)
	UPROPERTY(EditAnywhere, Category = "Dining|Preview",
		meta = (ClampMin = "0.1", Units = "min"))
	float BuffetWaitGameMinutes = 5.0f;

	// 이동이 끝나지 않을 때 복귀 또는 재시도할 월드 시간(초)
	UPROPERTY(EditAnywhere, Category = "Dining|Movement",
		meta = (ClampMin = "1.0", Units = "s"))
	float MovementTimeoutSeconds = 60.0f;

private:
	void UpdateQueue(double WorldNow);
	void BeginReturnToRoom();
	void ReleaseBuffetReservation();
	void ClearVisit();

	UPROPERTY(Transient)
	TWeakObjectPtr<ABuffetStation> Buffet;

	// 이동 중이거나 도착한 줄 위치. 앞 손님이 빠지면 새 위치로 이동한다.
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> QueueTarget;

	// 현재 진열대 예약 번호
	FGuid VisitId;

	// 현재 방문이 속한 투숙 번호
	FGuid VisitStayId;

	// 같은 투숙객이 하루에 한 번만 방문하도록 기록
	FGuid LastVisitStayId;
	int64 LastVisitDay = -1;

	double WaitUntilSeconds = -1.0;
	double MoveDeadlineWorldSeconds = -1.0;
	double NextReturnAttemptWorldSeconds = -1.0;
	double NextStartAttemptWorldSeconds = 0.0;

	bool bVisitActive = false;
	bool bReturning = false;
};
