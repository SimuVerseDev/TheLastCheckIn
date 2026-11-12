#pragma once

#include "CoreMinimal.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "ShadowAnomaly.generated.h"

class AShadowFigureActor;
class ARoomActor;

USTRUCT(BlueprintType)
struct FShadowDoorFigure
{
	GENERATED_BODY()
	
	UPROPERTY(EditInstanceOnly, Category="Night|Shadow")
	TObjectPtr<ARoomActor> Room;

	UPROPERTY(EditInstanceOnly, Category="Night|Shadow")
	TObjectPtr<AShadowFigureActor> Figure;
};

UCLASS()
class THELASTCHECKIN_API AShadowAnomaly : public AAnomalyBase
{
	GENERATED_BODY()
	
public:
	AShadowAnomaly();
	
	virtual bool StartAnomaly() override;
	virtual bool Escalate() override;
	virtual bool Fail() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/*
	로비, 객실 복도 장식 주변 형체 - 시작 후 RevealUntilHour 전까지 각자 무작위 시각에 하나씩 나타남
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Shadow")
	TArray<TObjectPtr<AShadowFigureActor>> DecorFigures;

	// 0이면 전부
	UPROPERTY(EditAnywhere, Category="Night|Shadow", meta=(ClampMin="0"))
	int32 DecorFigureCount = 3;

	// 이 시각(다음 날 새벽)까지 모든 형체가 나타남
	UPROPERTY(EditAnywhere, Category="Night|Shadow", meta=(ClampMin="0", ClampMax="23"))
	int32 RevealUntilHour = 4;

	/*
	투숙 중인 객실 문고리 형체 - 하룻밤 최대 1개, 나타나는 순간 컴플레인 전화
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Shadow")
	TArray<FShadowDoorFigure> DoorFigures;

	// 밤마다 문고리 형체가 나타날 확률
	UPROPERTY(EditAnywhere, Category="Night|Shadow", meta=(ClampMin="0", ClampMax="1"))
	float DoorFigureChance = 0.5f;

	UPROPERTY(EditAnywhere, Category="Night|Shadow")
	FText ComplaintMessage;

private:
	UFUNCTION()
	void HandleFigureAbsorbed(AShadowFigureActor* Figure);

	UFUNCTION()
	void HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds);

	void ShowFigure(AShadowFigureActor* Figure);
	void HideAllFigures();
	void TryResolve();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AShadowFigureActor>> ActiveFigures;

	// 아직 나타나지 않은 장식 형체와 나타날 게임 시각(초)
	UPROPERTY(Transient)
	TMap<TObjectPtr<AShadowFigureActor>, double> PendingReveals;

	// 문고리 형체가 나타날 게임 시각, 음수면 오늘 밤은 없음
	double DoorRevealSeconds = -1.0;

	// 컴플레인이 걸린 문고리 형체 - 끝까지 남아 있으면 평판 감소
	UPROPERTY(Transient)
	TObjectPtr<AShadowFigureActor> ComplaintFigure;
};