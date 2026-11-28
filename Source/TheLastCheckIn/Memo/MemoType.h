
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MemoType.generated.h"

UENUM(BlueprintType)
enum class EMemoState : uint8
{
	Locked, // 종이는 보이지만 아직 읽을 수 없음
	Unread, // 해금됨: 첫 열람 전까지 하이라이트
	Read    // 읽음: 하이라이트 없이 다시 열람 가능
};

USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FMemoDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Memo")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Memo",
		meta=(MultiLine="true"))
	FText Body;

	// 이 날짜 이상이어야 해금됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Memo",
		meta=(ClampMin="1"))
	int32 UnlockDay = 1;

	// None이면 사건 조건 없음.
	// 예: DayOne.LightWitnessed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Memo")
	FName RequiredEvent = NAME_None;

	// None이면 선행 메모 조건 없음.
	// 값이 있으면 해당 메모를 읽어야 해금됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Memo")
	FName RequiredReadMemo = NAME_None;
};