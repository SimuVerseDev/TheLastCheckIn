#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Housekeeping/Interaction/HKInteractable.h"
#include "HKPhysicsPickupActor.generated.h"

class UStaticMeshComponent;
UCLASS()
class AHKPhysicsPickupActor : public AActor, public IHKInteractable
{
	GENERATED_BODY()
	
public:
	AHKPhysicsPickupActor();
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Pickup")
	bool IsCarried() const
	{
		return Carrier.IsValid();
	}
	
	UStaticMeshComponent* GetMesh() const
	{
		return Mesh;
	}
	
	virtual void NotifyPickedUp(AActor* InCarrier);
	virtual void NotifyReleased(bool bThrown);
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh; // 물리 시뮬레이션 본체

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Housekeeping|Pickup")
	FText InteractPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Housekeeping|Pickup")
	bool bCanBePickedUp = true;

	
	/*
	C++에서 호출하고 BP에서 구현
	*/
	UFUNCTION(BlueprintImplementableEvent, Category="Housekeeping|Pickup", meta=(DisplayName="On Picked Up"))
	void BP_OnPickedUp(AActor* InCarrier);

	UFUNCTION(BlueprintImplementableEvent, Category="Housekeeping|Pickup", meta=(DisplayName="On Released"))
	void BP_OnReleased(bool bThrown);

private:

	TWeakObjectPtr<AActor> Carrier;
};