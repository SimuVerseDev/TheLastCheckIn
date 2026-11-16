#pragma once

#include "CoreMinimal.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "BellAnomaly.generated.h"

class ABellActor;
class ARoomActor;
class AHotelPhoneActor;

/*
로비 bell pull system - 빈 방이나 해금 안 된 방에 연결된 종 하나가 저절로 울림
방치하면 같은 층 투숙객이 컴플레인 전화, 종을 부수면 해결
*/
UCLASS()
class THELASTCHECKIN_API ABellAnomaly : public AAnomalyBase
{
	GENERATED_BODY()

public:
	ABellAnomaly();

	virtual bool StartAnomaly() override;
	virtual bool Escalate() override;
	virtual bool Fail() override;

protected:
	virtual void BeginPlay() override;

	// 비워 두면 월드의 모든 BellActor를 사용 (BP_bell_set의 Child Actor 포함)
	UPROPERTY(EditInstanceOnly, Category="Night|Bell")
	TArray<TObjectPtr<ABellActor>> Bells;

	// {Room}을 넣으면 컴플레인한 객실 번호로 바뀜
	UPROPERTY(EditAnywhere, Category="Night|Bell")
	FText ComplaintMessage;

	// 비워 두면 월드의 첫 번째 전화기를 사용
	UPROPERTY(EditInstanceOnly, Category="Night|Bell")
	TObjectPtr<AHotelPhoneActor> Phone;

private:
	UFUNCTION()
	void HandleBellBroken(ABellActor* Bell);

	ARoomActor* FindRoom(int32 RoomNumber) const;
	bool CanRing(const ABellActor* Bell) const;

	// 지금 울리고 있는 종 - 한 번에 하나
	UPROPERTY(Transient)
	TObjectPtr<ABellActor> RingingBell;
};
