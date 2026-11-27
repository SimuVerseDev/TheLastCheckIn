#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FoodDefinition.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EFoodCookState : uint8
{
    Raw,
    Cooking,
    Cooked
};

UCLASS(BlueprintType)
class THELASTCHECKIN_API UFoodDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
    FName FoodId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Appearance")
    TObjectPtr<UStaticMesh> RawMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Appearance")
    TObjectPtr<UStaticMesh> CookedMesh;

    // 음식의 실제 크기. 접시와 트레이의 배율을 상속하지 않는다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Appearance")
    FVector MeshScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Cooking")
    bool bRequiresCooking = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Cooking",
        meta = (ClampMin = "0.1", Units = "min"))
    float CookingGameMinutes = 5.0f;
};