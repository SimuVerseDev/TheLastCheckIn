#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HKStainActor.generated.h"
class UBoxComponent;
class UDecalComponent;
class UHKStainDirtComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AHKStainActor : public AActor
{
	GENERATED_BODY()
public:
	AHKStainActor();
	
	UHKStainDirtComponent* GetStain() const
	{
		return Stain;
	}
	
	FVector GetSurfaceNormal() const;
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> AimBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UHKStainDirtComponent> Stain;
	
	/*
	데칼 머티리얼에서 투명도를 조절하는 스칼라 파라미터 이름
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Stain")
	FName AmountParameter = FName("Opacity");

	UPROPERTY(EditAnywhere, Category="Housekeeping|Debug")
	bool bDirtyOnBeginPlay = false;

private:
	UFUNCTION()
	void HandleAmountChanged(float Amount);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMaterial;

	float BaseParameterValue = 1.0f;
};
