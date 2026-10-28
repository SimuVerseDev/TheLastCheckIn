#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "HKCarryComponent.generated.h"

class AHKBlanketActor;
class UPhysicsHandleComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnPickedUp, UPrimitiveComponent*, Component);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHKOnReleased, UPrimitiveComponent*, Component, bool, bThrown);

UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKCarryComponent : public UActorComponent, public IInteractionExtension
{
	GENERATED_BODY()

public:
	UHKCarryComponent();
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Carry")
	bool TryPickup(UPrimitiveComponent* Target);
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Carry")
	void Drop();

	UFUNCTION(BlueprintPure, Category="Housekeeping|Carry")
	bool IsCarrying() const
	{
		return CarriedComponent != nullptr;
	}
	UFUNCTION(BlueprintPure, Category="Housekeeping|Carry")
	UPrimitiveComponent* GetCarriedComponent() const
	{
		return CarriedComponent;
	}

	UPROPERTY(BlueprintAssignable, Category="Housekeeping|Carry")
	FHKOnPickedUp OnPickedUp;
	UPROPERTY(BlueprintAssignable, Category="Housekeeping|Carry")
	FHKOnReleased OnReleased;
	
	/*
	카트 장착 시 투입구 컴포넌트 지정, 해제 시 nullptr
	*/
	UFUNCTION(BlueprintPure, Category="Housekeeping|Trash")
	USceneComponent* GetTrashAbsorbTarget() const;
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Trash")
	void SetTrashAbsorbTarget(USceneComponent* Target)
	{
		TrashAbsorbTarget = Target;
	}
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Trash")
	bool CanAbsorbTrash() const
	{
		return GetTrashAbsorbTarget() != nullptr;
	}
	
	/*
	IInteractionExtension
	*/
	virtual AActor* FindInteractionTarget(const FVector& Start, const FVector& End) const override;
	virtual bool BlocksFocus() const override
	{
		return IsCarrying();
	}
	virtual FText GetBlockingPrompt() const override;
	virtual void OnUseReleased() override
	{
		Drop();
	}
	virtual void GetIgnoredActors(TArray<AActor*>& IgnoredActors) const override; // InteractionExtension.h
	
protected:
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm"))
	float HoldDistance = 150.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm"))
	float BreakDistance = 400.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="kg"))
	float MaxCarryMass = 50.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0))
	float ThrowVelocityScale = 1.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm/s"))
	float MaxThrowSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm/s"))
	float ThrowSpeedThreshold = 300.0f;
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/*
	이불을 시선으로 잡을 수 있는 반경
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm"))
	float BlanketGrabRadius = 30.0f;

	/*
	손에 든 도구에서 이 태그가 붙은 컴포넌트를 흡수 투입구로 인식
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Housekeeping|Debug")
	FName TrashAbsorbPointTag = FName("TrashAbsorbPoint");

private:
	bool GetViewTransform(FVector& OutHoldLocation, FRotator& OutViewRotation) const;
	void Release(bool bInheritHandVelocity);
	
	/*
	댕글링 포인터 방지용 UPROPERTY : Transient 지정자를 포함한 UPROPERTY는 휘발성 데이터로 지정
	*/
	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> CarriedComponent;

	TWeakObjectPtr<AHKBlanketActor> CarriedBlanket;
	TWeakObjectPtr<USceneComponent> TrashAbsorbTarget;
	
	FQuat RotationOffset = FQuat::Identity; // 몸을 돌리면 물체 방향도 같이 따라올 수 있도록 내 시선 기준으로 물체가 어떻게 돌아가는지 저장
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
	FVector LastHoldLocation = FVector::ZeroVector;
	FVector HoldVelocity = FVector::ZeroVector;
};