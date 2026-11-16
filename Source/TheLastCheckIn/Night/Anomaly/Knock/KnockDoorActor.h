#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnockDoorActor.generated.h"

class UAudioComponent;
class USoundBase;
class ARoomActor;
class AKnockDoorActor;
class AHorseshoeActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKnockDoorHorseshoePlaced, AKnockDoorActor*, Door); // 말발굽 놓기 감지

/*
문 앞 노크 지점 (소리 + 말발굽 자리)
문 여닫기는 레벨의 BP_Door가 담당
*/
UCLASS()
class THELASTCHECKIN_API AKnockDoorActor : public AActor
{
	GENERATED_BODY()

public:
	AKnockDoorActor();

	/*
	CCTV실·직원실: LinkedDoor(BP_Door)의 bIsOpen
	*/
	UFUNCTION(BlueprintPure, Category="Night|Knock")
	bool IsOpen() const;

	UFUNCTION(BlueprintPure, Category="Night|Knock")
	ARoomActor* GetRoom() const
	{
		return Room;
	}

	UFUNCTION(BlueprintCallable, Category="Night|Knock")
	void PlayKnock();

	UFUNCTION(BlueprintCallable, Category="Night|Knock")
	void StopKnock();

	UFUNCTION(BlueprintCallable, Category="Night|Knock")
	void SetAwaitingHorseshoe(bool bInAwaiting)
	{
		bAwaitingHorseshoe = bInAwaiting;
	}

	bool TryPlaceHorseshoe(AHorseshoeActor* Horseshoe);

	UFUNCTION(BlueprintPure, Category="Night|Knock")
	bool CanOpen() const
	{
		return bCanOpen;
	}

	UPROPERTY(BlueprintAssignable, Category="Night|Knock")
	FKnockDoorHorseshoePlaced OnHorseshoePlaced;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAudioComponent> KnockAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> DropPoint;

	/*
	CCTV실·직원실 = true, 객실 = false
	*/
	UPROPERTY(EditInstanceOnly, Category="Night|Knock")
	bool bCanOpen = true;
	
	UPROPERTY(EditInstanceOnly, Category="Night|Knock", meta=(EditCondition="bCanOpen"))
	TSoftObjectPtr<AActor> LinkedDoor; // 문이 다른 레벨(Level_combine)에 있어서 Soft 참조
	
	UPROPERTY(EditInstanceOnly, Category="Night|Knock", meta=(EditCondition="!bCanOpen"))
	TObjectPtr<ARoomActor> Room;

	UPROPERTY(EditAnywhere, Category="Night|Knock")
	TObjectPtr<USoundBase> KnockSound;

	UPROPERTY(EditAnywhere, Category="Night|Knock", meta=(ClampMin="10", Units="cm"))
	float DropRadius = 100.0f;

private:
	bool bAwaitingHorseshoe = false;
};
