#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RoomDataTypes.generated.h"

// 객실의 현재 상태
UENUM(BlueprintType)
enum class ERoomState : uint8 
{
	Available UMETA(DisplayName = "Available"), //배정 가능
	Assigned UMETA(DisplayName = "Assigned"), //배정 완료
	Occupied UMETA(DisplayName = "Occupied"), //투숙 중
	//NeedsCleaning UMETA(DisplayName = "NeedsCleaning")
};

//객실 종류별 기본 설정
USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FRoomTypeRow : public FTableRowBase
{
	GENERATED_BODY()
	
	//최대 숙박 인원
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room",
		meta =(ClampMin="1"))
	int32 Capacity = 1;
	
	//기본 숙박 요금
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room",
		 meta = (ClampMin = "0.0"))
	float BaseRoomRate = 100.0f;
};


