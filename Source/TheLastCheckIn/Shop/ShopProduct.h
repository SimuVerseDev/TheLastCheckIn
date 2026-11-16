#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "ShopProduct.generated.h"

class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EShopProductCategory : uint8
{
	Furniture,
	Tool
};

/** Row name is the stable product ID. Preview mesh must match the actor's pivot and size. */
USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FShopProductRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EShopProductCategory Category = EShopProductCategory::Furniture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 Price = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;
	// Required: an inert visual used during placement. No product Blueprint runs before confirmation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> PreviewMesh;
	// Optional: empty creates a static mesh actor using PreviewMesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<AActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float Scale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAvailable = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SortOrder = 0;
};

/** Configure the catalog without modifying a map or placed actor. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Hotel Shop"))
class THELASTCHECKIN_API UShopSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Products", meta=(RequiredAssetDataTags="RowStructure=/Script/TheLastCheckIn.ShopProductRow"))
	TSoftObjectPtr<UDataTable> ProductTable;
};
