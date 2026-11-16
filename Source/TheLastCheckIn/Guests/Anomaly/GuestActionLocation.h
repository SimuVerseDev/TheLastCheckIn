#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GuestActionLocation.generated.h"

class UBoxComponent;
class UArrowComponent;
class UAnimMontage;
class USoundBase;
class AGuestCharacter;

// 로비나 각 층 복도처럼 이상행동이 발생할 수 있는 구역
UCLASS(Blueprintable)
class THELASTCHECKIN_API AGuestActionZone : public AActor
{
    GENERATED_BODY()

public:
    AGuestActionZone();

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Anomaly|Zone")
    TObjectPtr<UBoxComponent> Bounds;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Anomaly|Zone")
    bool bEnabled = true;

    UFUNCTION(BlueprintPure, Category = "Anomaly|Zone")
    bool ContainsLocation(FVector Location) const;

    // 플레이어가 없는 구역에서만 새로운 행동을 배정
    UFUNCTION(BlueprintPure, Category = "Anomaly|Zone")
    bool CanStartAction() const;
};

// 손님이 서서 행동할 위치와 방향
UCLASS(Blueprintable)
class THELASTCHECKIN_API AGuestActionPoint : public AActor
{
    GENERATED_BODY()

public:
    AGuestActionPoint();

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Anomaly|Point")
    TObjectPtr<UArrowComponent> Facing;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "Anomaly|Point")
    TObjectPtr<AGuestActionZone> Zone;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Anomaly|Point")
    bool bEnabled = true;

    // 나중에 연결할 연출 데이터이며 자동 재생하지는 않음
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Anomaly|Presentation")
    TObjectPtr<UAnimMontage> ActionMontage;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Anomaly|Presentation")
    TObjectPtr<USoundBase> NoiseSound;

    // 새로운 손님에게 배정 가능한 지점인지 검사
    UFUNCTION(BlueprintPure, Category = "Anomaly|Point")
    bool IsAvailable() const;

    bool TryReserve(UObject* Requester);
    bool IsReservedBy(const UObject* Requester) const;

    // 도착한 손님이 예약한 지점에서 행동을 시작할 수 있는지 검사
    // 이 단계에서는 플레이어 유무를 다시 검사하지 않음
    bool CanBeginAction(const UObject* Requester) const;

    void Release(UObject* Requester);

    // 실제 도착 후 한 번 호출
    UFUNCTION(
        BlueprintImplementableEvent,
        Category = "Anomaly|Presentation")
    void OnActionStarted(AGuestCharacter* Guest);

    // 제압되거나 사건이 종료되면 호출
    UFUNCTION(
        BlueprintImplementableEvent,
        Category = "Anomaly|Presentation")
    void OnActionStopped(AGuestCharacter* Guest);

private:
    // 플레이어 유무와 별개로 지점과 구역 설정 자체가 유효한지 검사
    bool HasValidPlacement() const;

    // 다른 사건에서도 같은 지점을 동시에 사용하지 못하도록 예약
    TWeakObjectPtr<UObject> Reservation;
};