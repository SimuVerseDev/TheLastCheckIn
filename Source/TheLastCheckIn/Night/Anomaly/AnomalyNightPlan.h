#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AnomalyNightPlan.generated.h"

UENUM(BlueprintType)
enum class EScheduleEventKind : uint8
{
	Anomaly, // 이상현상 시작 (EventId = AAnomalyBase::AnomalyId)
	Omen,    // 예고 재생 (EventId = 예고할 이상현상의 AnomalyId)
	Scare    // 공포 연출 재생 (EventId = AScareBase::ScareId, 같은 ID 전부 실행)
};

USTRUCT(BlueprintType)
struct THELASTCHECKIN_API FAnomalyScheduleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Schedule", meta = (ClampMin = "1"))
	int32 Day = 2;

	// 0~23, DayStartHour(06시) 이전 시각은 그날 밤(다음 날 새벽)으로 계산
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Schedule", meta = (ClampMin = "0", ClampMax = "23"))
	int32 Hour = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Schedule", meta = (ClampMin = "0", ClampMax = "59"))
	int32 Minute = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Schedule")
	EScheduleEventKind Kind = EScheduleEventKind::Anomaly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Schedule")
	FName EventId;

	// 비우면 이상현상이 알아서 대상 선택 (나중에 장소 고정용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Schedule")
	FName TargetId;
};
