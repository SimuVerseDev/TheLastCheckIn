#pragma once

#include "CoreMinimal.h"
#include "Night/Anomaly/AnomalyBase.h"
#include "KnockAnomaly.generated.h"

class AKnockDoorActor;
class AHotelPhoneActor;
class ACCTVMonitor;

UCLASS()
class THELASTCHECKIN_API AKnockAnomaly : public AAnomalyBase
{
	GENERATED_BODY()
	
public:
	AKnockAnomaly();

	virtual bool StartAnomaly() override;
	virtual bool Fail() override;
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, Category="Night|Knock")
	TArray<TObjectPtr<AKnockDoorActor>> Doors;
	
	UPROPERTY(EditInstanceOnly, Category="Night|Knock")
	TObjectPtr<AHotelPhoneActor> Phone;

	UPROPERTY(EditInstanceOnly, Category="Night|Knock")
	TObjectPtr<ACCTVMonitor> CCTVMonitor;
	
	UPROPERTY(EditAnywhere, Category="Night|Knock")
	TArray<FText> ComplaintMessages;
	
	UPROPERTY(EditInstanceOnly, Category="Night|Knock", meta=(ClampMin="0.5", Units="s"))
	float KnockInterval = 4.0f;
	
private:
	UFUNCTION()
	void HandleHorseshoePlaced(AKnockDoorActor* Door);

	void TickKnock();
	void ClearActiveDoor();

	UPROPERTY(Transient)
	TObjectPtr<AKnockDoorActor> ActiveDoor;

	FTimerHandle KnockTimer;
	int32 ComplaintIndex = 0;
};