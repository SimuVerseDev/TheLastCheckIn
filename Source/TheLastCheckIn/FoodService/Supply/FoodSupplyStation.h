#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "FoodSupplyStation.generated.h"

class UBoxComponent;
class UFoodDefinition;
class UInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class THELASTCHECKIN_API AFoodSupplyStation : public AActor, public IInteractable
{
    GENERATED_BODY()

public:
    AFoodSupplyStation();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual bool CanUse_Implementation(AActor* Interactor) const override;
    virtual FText GetUsePrompt_Implementation(AActor* Interactor) const override;
    virtual bool Use_Implementation(AActor* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> StationMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionBounds;

    // 진열 음식은 장식용이며 꺼내도 줄어들지 않는다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food|Supply")
    TObjectPtr<UInstancedStaticMeshComponent> DisplayFood;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food|Supply")
    TObjectPtr<USceneComponent> PickupPoint;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Supply")
    TObjectPtr<UFoodDefinition> FoodDefinition;

    // 공급대 기준 진열 위치. 크기는 음식 데이터의 MeshScale에 곱해진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Supply")
    TArray<FTransform> DisplayTransforms;

private:
    bool bDispensing = false;
};
