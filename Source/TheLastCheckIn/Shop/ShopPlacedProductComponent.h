#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "ShopPlacedProductComponent.generated.h"

class UStaticMesh;

/** Runtime identity and reversible movement of a purchased actor, without respawning its Blueprint. */
UCLASS(ClassGroup=(Shop))
class THELASTCHECKIN_API UShopPlacedProductComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void InitializeProduct(FName InProductId, UStaticMesh* InPreviewMesh);
	UFUNCTION(BlueprintPure, Category="Shop") FName GetProductId() const { return ProductId; }
	UFUNCTION(BlueprintPure, Category="Shop") bool CanRelocate() const;
	UStaticMesh* GetPreviewMesh() const { return PreviewMesh; }
	bool BeginRelocation();
	// The caller validates the destination. No economy or spawn operation occurs here.
	bool FinishRelocation(const FTransform& Destination);
	void CancelRelocation();
	bool IsRelocating() const { return bRelocating; }
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY() FName ProductId;
	UPROPERTY() TObjectPtr<UStaticMesh> PreviewMesh;
	struct FSceneState
	{
		TWeakObjectPtr<USceneComponent> Component;
		TWeakObjectPtr<USceneComponent> Parent;
		FName Socket;
		FTransform WorldTransform;
		EComponentMobility::Type Mobility;
		int32 AttachmentDepth = 0;
		bool bSimulating = false;
		FVector LinearVelocity = FVector::ZeroVector;
		FVector AngularVelocity = FVector::ZeroVector;
	};
	TArray<FSceneState> SceneStates;
	TArray<TWeakObjectPtr<UActorComponent>> TickingComponents;
	FTransform OriginalTransform;
	bool bOriginalHidden = false;
	bool bOriginalCollision = false;
	bool bOriginalTick = false;
	bool bRelocating = false;
	void Restore(const FTransform& Destination, bool bCancelled);
};
