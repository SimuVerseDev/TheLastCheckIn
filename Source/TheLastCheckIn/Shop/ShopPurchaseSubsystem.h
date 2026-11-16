#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Shop/ShopProduct.h"
#include "ShopPurchaseSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShopPurchaseConfirmed, FName, ProductId, AActor*, Product);

/** Single-player purchase reservation. Money is debited once and refunded once on cancellation. */
UCLASS()
class THELASTCHECKIN_API UShopPurchaseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable, Category="Shop") bool BeginPurchase(UDataTable* Table, FName ProductId, FText& Error);
	// Caller supplies a validated placement transform. Failure cancels and refunds the reservation.
	UFUNCTION(BlueprintCallable, Category="Shop") AActor* ConfirmPurchase(const FTransform& Transform, FText& Error);
	UFUNCTION(BlueprintCallable, Category="Shop") bool CancelPurchase();
	UFUNCTION(BlueprintPure, Category="Shop") bool HasPendingPurchase() const { return bPending; }
	UFUNCTION(BlueprintPure, Category="Shop") FName GetPendingProductId() const { return PendingId; }
	UFUNCTION(BlueprintPure, Category="Shop") int32 GetReservedPrice() const { return bPending ? PendingProduct.Price : 0; }
	const FShopProductRow& GetPendingProduct() const { return PendingProduct; }
	UPROPERTY(BlueprintAssignable, Category="Shop") FShopPurchaseConfirmed OnPurchaseConfirmed;
private:
	UPROPERTY() TObjectPtr<class UHotelEconomySubsystem> Economy;
	UPROPERTY() FShopProductRow PendingProduct;
	UPROPERTY() TObjectPtr<UClass> PendingClass;
	UPROPERTY() TObjectPtr<UStaticMesh> PendingMesh;
	FName PendingId;
	bool bPending = false;
	bool bChanging = false;
	void ClearPending();
};
