#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "CheckInCounter.generated.h"


class AGuestCharacter;
class UGuestParty;
/*
체크인 카운터 : 공통 상호작용(E)으로 체크인 UI 열기
*/
UCLASS()
class THELASTCHECKIN_API ACheckInCounter : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	/*
	플레이어가 카운터 영역 안에 있고 대기 손님이 있으면 E 가능
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	/*
	E 안내 문구
	*/
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;

	/*
	E : 체크인 UI 열기
	*/
	virtual void Interact_Implementation(AActor* Interactor) override;
	
	bool HasSlots(int32 RequiredCount) const;
	bool CanAcceptParty() const;

	bool RegisterParty(UGuestParty* InParty);
	void UnregisterParty(UGuestParty* InParty);

	UGuestParty* GetCurrentParty() const;

	bool GetSlotTransformForGuest(
		const AGuestCharacter* Guest,
		FTransform& OutTransform) const;
	
private:
	bool GetSlotTransform(
		int32 SlotIndex,
		FTransform& OutTransform) const;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "CheckIn")
	TWeakObjectPtr<UGuestParty> CurrentParty;
};
