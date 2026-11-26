
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MemoType.generated.h"

UENUM(BlueprintType)
enum class EMemoState : uint8
{
	Locked, // 보이지 않음
	Unread, // 활성화되었지만 아직 읽지 않음
	Read    // 읽었으며 다시 열람 가능
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