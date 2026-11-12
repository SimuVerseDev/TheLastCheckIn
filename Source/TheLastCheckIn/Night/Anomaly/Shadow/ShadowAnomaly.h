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
	
	/*
	미리 배치된 이상현상
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Shadow")
	TArray<TObjectPtr<AShadowFigureActor>> DecorFigures;
	
	UPROPERTY(EditAnywhere, Category="Night|Shadow", meta=(ClampMin="0"))
	int32 DecorFigureCount = 3;
	
	/*
	방치 시 투숙 중인 객실 문 앞에 생길 형체
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Shadow")
	TArray<FShadowDoorFigure> DoorFigures;
	
	UPROPERTY(EditAnywhere, Category="Night|Shadow")
	FText ComplaintMessage;
	
private:
	UFUNCTION()
	void HandleFigureAbsorbed(AShadowFigureActor* Figure);

	void ShowFigure(AShadowFigureActor* Figure);
	void HideAllFigures();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AShadowFigureActor>> ActiveFigures;
};