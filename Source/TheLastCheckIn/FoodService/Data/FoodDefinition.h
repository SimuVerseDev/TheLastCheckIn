#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FoodDefinition.generated.h"

class UStaticMesh;

// 음식 표시 형태
UENUM(BlueprintType)
enum class EFoodPresentation : uint8
{
    Batch,      // 조리·운반·진열용 음식. 1인분 이상.
    Serving     // 손님에게 제공할 1인분 접시
};

// 음식 종류별 이름, 메시, 조리 설정
UCLASS(BlueprintType)
class THELASTCHECKIN_API UFoodDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // 음식 종류를 구분하는 ID. 예: Bread, Salad
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
    FName FoodId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food")
    FText DisplayName;

    // 운반·진열할 음식 메시
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Appearance")
    TObjectPtr<UStaticMesh> BatchMesh;

    // 손님에게 줄 1인분 음식과 접시 메시
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Appearance")
    TObjectPtr<UStaticMesh> ServingMesh;

    // 한 번 조리할 때 만드는 인분 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Cooking",
        meta = (ClampMin = "1"))
    int32 ServingsPerBatch = 1;

    // 조리 시간. 게임 시간 기준 분 단위이며, 조리 기능에서 사용할 설정.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Cooking",
        meta = (ClampMin = "0.1", Units = "min"))
    float CookingGameMinutes = 5.0f;
};
