#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HKCarryComponent.generated.h"

class UPhysicsHandleComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHKOnPickedUp, UPrimitiveComponent*, Component);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHKOnReleased, UPrimitiveComponent*, Component, bool, bThrown);

UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHKCarryComponent();
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Carry")
	bool TryPickup(UPrimitiveComponent* Target);
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Carry")
	void Drop();
	
	UFUNCTION(BlueprintCallable, Category="Housekeeping|Carry")
	void Throw();
	
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

protected:
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm"))
	float HoldDistance = 150.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm"))
	float BreakDistance = 400.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="kg"))
	float MaxCarryMass = 50.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Carry", meta=(ClampMin=0, Units="cm/s"))
	float ThrowSpeed = 1200.0f;
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	bool GetViewTransform(FVector& OutHoldLocation, FRotator& OutViewRotation) const;
	void Release(bool bThrown);
	
	/*
	댕글링 포인터 방지용 transient : Transient 지정자를 포함한 UPROPERTY는 휘발성 데이터로 지정
	*/
	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> CarriedComponent;
	
	FQuat RotationOffset = FQuat::Identity; // 몸을 돌리면 물체 방향도 같이 따라올 수 있도록 내 시선 기준으로 물체가 어떻게 돌아가는지 저장
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};