#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "HKWipeComponent.generated.h"
class AHKStainActor;
class APlayerController;
class UHKWipeToolComponent;
class UPrimitiveComponent;

UCLASS(ClassGroup=(Housekeeping), meta=(BlueprintSpawnableComponent))
class UHKWipeComponent : public UActorComponent, public IInteractionExtension
{
	GENERATED_BODY()

public:
	UHKWipeComponent();

	bool BeginWipe(AHKStainActor* InStain, UHKWipeToolComponent* InTool);
	void EndWipe();

	UFUNCTION(BlueprintPure, Category="Housekeeping|Wipe")
	bool IsWiping() const
	{
		return Stain.IsValid();
	}

	virtual bool BlocksFocus() const override
	{
		return IsWiping();
	}
	virtual void OnUseReleased() override
	{
		EndWipe();
	}

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Wipe", meta=(ClampMin=0, Units="cm"))
	float MouseSensitivity = 2.0f;

private:
	UPrimitiveComponent* GetToolBody() const;
	APlayerController* GetPlayerController() const;

	TWeakObjectPtr<AHKStainActor> Stain;
	TWeakObjectPtr<UHKWipeToolComponent> Tool;
	FVector2D WipeOffset = FVector2D::ZeroVector;
};
