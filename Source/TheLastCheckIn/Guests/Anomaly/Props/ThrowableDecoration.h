#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThrowableDecoration.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UArrowComponent;
class APawn;

UCLASS(Blueprintable)
class THELASTCHECKIN_API AThrowableDecoration : public AActor
{
    GENERATED_BODY()

public:
    AThrowableDecoration();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoration")
    TObjectPtr<UStaticMeshComponent> PropMesh;

    // 손님이 물건을 바라볼 방향. 물건마다 별도의 타겟포인트를 배치하지 않는다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decoration|Approach")
    TObjectPtr<UArrowComponent> ApproachDirection;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Approach",
        meta = (ClampMin = "10.0", Units = "cm"))
    float ApproachDistance = 80.0f;

    // 손님 발밑 기준: X 앞, Y 오른쪽, Z 위. 물건의 스케일과 무관한 거리.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Pickup")
    FVector PickupOffset = FVector(70.0f, 0.0f, 5.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Pickup")
    FRotator PickupRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Decoration|Approach",
        meta = (ClampMin = "1.0"))
    FVector NavigationProjectionExtent = FVector(30.0f, 30.0f, 200.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Decoration|Approach",
        meta = (ClampMin = "0.0", Units = "cm"))
    float MaxApproachProjectionError = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Knock",
        meta = (ClampMin = "0.0"))
    float KnockForwardSpeed = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Knock",
        meta = (ClampMin = "0.0"))
    float KnockUpSpeed = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Throw",
        meta = (ClampMin = "0.0"))
    float ThrowForwardSpeed = 550.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Throw",
        meta = (ClampMin = "0.0"))
    float ThrowUpSpeed = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Grip")
    FName GripSocket = TEXT("PropGrip_R");

    // 손 소켓에 붙인 뒤 물건별로 조정할 위치와 회전
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Grip")
    FVector GripLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Grip")
    FRotator GripRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration|Physics")
    FVector KnockAngularVelocity = FVector(0.0f, 180.0f, 0.0f);

    bool CanReserve() const;
    bool TryReserve(UObject* Requester);
    bool IsReservedBy(const UObject* Requester) const;

    bool TryBuildActionTransform(const APawn* Guest, FTransform& OutTransform) const;

    bool KnockDown(UObject* Requester, const FVector& Velocity);
    bool PreparePickup(UObject* Requester, const FTransform& PickupTransform);
    bool Grab(UObject* Requester, USkeletalMeshComponent* HandMesh);
    bool Throw(UObject* Requester, const FVector& Velocity);

    // 원래 위치로 복귀시키지 않고 물건 사용만 종료
    void EndInteraction(UObject* Requester);

private:
    bool EnablePhysics();
    void ClearPhysicsVelocity();

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> ReservedBy;

    bool bHeld = false;
    bool bPreparedForPickup = false;
};
