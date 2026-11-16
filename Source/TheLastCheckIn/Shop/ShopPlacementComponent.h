#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Core/GameFlow/Enums/HotelPhaseTypes.h"
#include "ShopPlacementComponent.generated.h"

class UDataTable;
class UShopPurchaseSubsystem;
class UTextBlock;
class AStaticMeshActor;
class UShopPlacementComponent;
class UShopPlacedProductComponent;

/** Non-interactive placement instructions; gameplay retains movement and camera input. */
UCLASS()
class THELASTCHECKIN_API UShopPlacementWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UShopPlacementComponent> Placement;
	void SetStatus(const FText& Text);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> Status;
};

/** Installed on the native player controller; no level actor or Level Blueprint is required. */
UCLASS(ClassGroup=(Shop), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UShopPlacementComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UShopPlacementComponent();
	UFUNCTION(BlueprintCallable, Category="Shop")
	bool PurchaseAndPlace(UDataTable* Table, FName ProductId, UUserWidget* ShopToClose, FText& Error);
	UFUNCTION(BlueprintCallable, Category="Shop") bool ConfirmPlacement();
	UFUNCTION(BlueprintCallable, Category="Shop") void CancelPlacement();
	UFUNCTION(BlueprintCallable, Category="Shop") void RotatePreview(float Degrees);
	UFUNCTION(BlueprintPure, Category="Shop") bool IsPlacing() const { return Preview != nullptr; }
	bool HasPlacementInteraction() const { return IsPlacing() || bHoldingProduct; }
	bool HandlePlacementPress();
	bool HandlePlacementRelease();
	UPROPERTY(EditAnywhere, Category="Shop|Relocation", meta=(ClampMin="0.1", Units="s"))
	float RelocationHoldSeconds = 0.5f;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
	UPROPERTY() TObjectPtr<UShopPurchaseSubsystem> Purchase;
	UPROPERTY() TObjectPtr<AStaticMeshActor> Preview;
	UPROPERTY() TObjectPtr<UShopPlacementWidget> Controls;
	TWeakObjectPtr<APawn> PlacementPawn;
	TSharedPtr<class IInputProcessor> PlacementInput;
	TWeakObjectPtr<UShopPlacedProductComponent> HoldCandidate;
	UPROPERTY() TObjectPtr<UShopPlacedProductComponent> RelocatingProduct;
	bool bHoldingProduct = false;
	float HoldElapsed = 0;
	bool bChangedCursor = false;
	bool bAwaitingFailureDismissal = false;
	bool bOwnsPurchase = false;
	bool bPreviousCursor = false;
	bool bValidPlacement = false;
	float Yaw = 0;
	void UpdatePreview();
	void EnsurePlacementInput();
	bool CreatePreview(UStaticMesh* Mesh, const FVector& Scale);
	bool OpenControls();
	UShopPlacedProductComponent* FindRelocationTarget() const;
	bool BeginRelocation();
	void UpdateRelocationHold(float DeltaTime);
	friend class FShopRelocationInputTest;
	void ClosePlacement();
	UFUNCTION() void HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase);
};
