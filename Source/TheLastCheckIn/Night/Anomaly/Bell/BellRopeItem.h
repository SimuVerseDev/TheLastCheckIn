#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "BellRopeItem.generated.h"

class UStaticMeshComponent;

/*
종을 묶는 줄 - E로 들고 내려놓음
들고 있는 동안 울리는 종을 보면 LMB로 묶을 수 있음 (BellActor::Use), 써도 없어지지 않음
*/
UCLASS()
class THELASTCHECKIN_API ABellRopeItem : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ABellRopeItem();

	// Interactor가 들고 있는 줄, 없으면 nullptr
	static ABellRopeItem* FindHeldBy(const AActor* Interactor);

	UFUNCTION(BlueprintPure, Category="Night|Bell|Rope")
	bool IsHeld() const
	{
		return Holder.IsValid();
	}

	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Bell|Rope")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/*
	손에 쥐는 위치 - 1인칭 팔 메시의 소켓 기준, 소켓이 없으면 HeldOffset으로 카메라에 붙음
	*/
	UPROPERTY(EditAnywhere, Category="Night|Bell|Rope|Hold")
	FName HandSocket = TEXT("HandGrip_R");

	UPROPERTY(EditAnywhere, Category="Night|Bell|Rope|Hold")
	FTransform HandTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category="Night|Bell|Rope|Hold")
	FTransform HeldOffset = FTransform(FRotator::ZeroRotator, FVector(45.0f, 20.0f, -30.0f));

	// 내려놓을 때 시선 방향으로 이 거리 안에서 바라보는 곳에 놓음
	UPROPERTY(EditAnywhere, Category="Night|Bell|Rope|Hold", meta=(ClampMin="0", Units="cm"))
	float DropDistance = 250.0f;

	UPROPERTY(EditAnywhere, Category="Night|Bell|Rope")
	FText PickupPrompt;

	UPROPERTY(EditAnywhere, Category="Night|Bell|Rope")
	FText DropPrompt;

private:
	void Equip(AActor* Interactor);
	void Unequip();
	void SetFirstPersonRender(bool bFirstPerson);

	TWeakObjectPtr<AActor> Holder;

	// 손에 들기 전 크기 - 내려놓을 때 되돌림
	FVector WorldScale = FVector::OneVector;
};
