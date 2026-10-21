#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GuestDataTypes.generated.h"

//손님의 현재 행동 단계
UENUM(BlueprintType)
enum class EGuestState : uint8 
{
	MovingToCounter UMETA(DisplayName = "Moving To Counter"),
	WaitingForCheckIn UMETA(DisplayName = "Waiting for Check In"),
	MovingToRoom UMETA(DisplayName = "Moving To Room"),
	Staying UMETA(DisplayName = "Staying")
	
};

// 손님의 퇴실 방식 선택지 (자동, 플레이어의 요청, 랜덤)
UENUM(BlueprintType)
enum class EGuestCheckoutPolicy : uint8
{
	Automatic UMETA(DisplayName = "Automatic"),
	RequireRequest UMETA(DisplayName = "Require Request"),
	Random UMETA(DisplayName = "Random")
};

// 행동 설정 테이블
USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FGuestBehaviorRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkout")
	EGuestCheckoutPolicy CheckoutPolicy = EGuestCheckoutPolicy::Automatic;
	
	// 손님의 잔류 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkout", 
		meta=(ClampMin ="0.0", ClampMax ="1.0"))
	float StayBehindChance = 0.0f;
};

// 손님 유형 테이블
USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FGuestTypeRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement",
		meta = (ClampMin ="0.0"))
	float WalkSpeed =200.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	FDataTableRowHandle BehaviorRow;
};


