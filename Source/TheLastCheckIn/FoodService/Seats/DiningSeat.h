#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiningSeat.generated.h"

class ATargetPoint;
class UArrowComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class THELASTCHECKIN_API ADiningSeat : public AActor
{
    GENERATED_BODY()

public:
    ADiningSeat();

    // 비어 있는 좌석을 예약한다.
    bool TryReserve(AActor* Guest, const FGuid& ReservationId);

    // 해당 손님이 예약한 좌석인지 확인한다.
    bool IsReservedBy(const AActor* Guest, const FGuid& ReservationId) const;

    // 예약한 손님과 예약 번호가 일치할 때만 해제한다.
    void Release(const AActor* Guest, const FGuid& ReservationId);

    ATargetPoint* GetApproachPoint() const;

    FTransform GetSeatTransform() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dining")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dining")
    TObjectPtr<UStaticMeshComponent> ChairMesh;

    // 착석할 캡슐 중심 위치와 손님이 바라볼 방향.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dining")
    TObjectPtr<UArrowComponent> SeatPoint;

    // 의자에 앉기 전과 일어난 직후에 서 있을 위치.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Dining")
    TObjectPtr<ATargetPoint> ApproachPoint;

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> ReservedGuest;

    FGuid ActiveReservationId;
};
