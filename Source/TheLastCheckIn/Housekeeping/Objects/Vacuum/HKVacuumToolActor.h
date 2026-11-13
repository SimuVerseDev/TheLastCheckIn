#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "HKVacuumToolActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UCameraComponent;

UCLASS()
class THELASTCHECKIN_API AHKVacuumToolActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AHKVacuumToolActor();

	static AHKVacuumToolActor* FindHeldBy(const AActor* Interactor);

	USceneComponent* GetNozzle() const
	{
		return Nozzle;
	}

	/*
	흡입 대상이 날아갈 위치 - 손에 든 청소기는 1인칭 렌더(FOV, 스케일)로 그려지므로
	화면에 보이는 흡입구 위치로 보정한 월드 좌표
	*/
	FVector GetSuckTargetLocation() const;

	// 흡입 연출 시작 - 청소기 떨림과 카메라 진동
	void BeginSuck(float Duration);

	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Housekeeping|Vacuum")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/*
	흡입구
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Housekeeping|Vacuum")
	TObjectPtr<USceneComponent> Nozzle;

	/*
	손에 쥐는 위치 - 1인칭 팔 메시의 소켓 기준, 소켓이 없으면 HeldOffset으로 카메라에 붙음
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Hold")
	FName HandSocket = TEXT("HandGrip_R");

	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Hold")
	FTransform HandTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Hold")
	FTransform HeldOffset = FTransform(FRotator::ZeroRotator, FVector(60.0f, 25.0f, -40.0f));

	// 집어 들 때 놓인 자리에서 손까지 끌려오는 시간
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Hold", meta=(ClampMin="0", Units="s"))
	float EquipBlendTime = 0.25f;

	// 시선을 돌릴 때 청소기가 늦게 따라오는 정도(도)
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Hold", meta=(ClampMin="0"))
	float SwayAmount = 4.0f;

	/*
	흡입 연출
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Suck", meta=(ClampMin="0", Units="deg"))
	float CameraShakeStrength = 0.8f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum|Suck", meta=(ClampMin="0", Units="cm"))
	float SuckJitter = 0.6f;

	// HKBlanketActor.h - FText GrabPrompt
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum")
	FText PickupPrompt;
	UPROPERTY(EditAnywhere, Category="Housekeeping|Vacuum")
	FText DropPrompt;

private:
	void Equip(AActor* Interactor);
	void Unequip();
	void SetFirstPersonRender(bool bFirstPerson);
	FTransform GetHoldTransform() const;

	TWeakObjectPtr<AActor> Holder;
	TWeakObjectPtr<UCameraComponent> HolderCamera;
	bool bInHand = false;

	FTransform EquipStart;
	float EquipElapsed = 0.0f;

	FRotator LastControlRotation = FRotator::ZeroRotator;
	FRotator Sway = FRotator::ZeroRotator;

	float SuckElapsed = 0.0f;
	float SuckDuration = 0.0f;
};
