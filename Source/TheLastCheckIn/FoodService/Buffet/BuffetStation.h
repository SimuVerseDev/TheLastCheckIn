#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "BuffetStation.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UArrowComponent;
class UFoodDefinition;
class UFoodSnapSlotComponent;
class AFoodItemActor;
class AFoodPlateActor;
class ATargetPoint;

UCLASS()
class THELASTCHECKIN_API ABuffetStation
    : public AActor
    , public IInteractable
{
    GENERATED_BODY()

public:
    ABuffetStation();

    /*
    Interact : E키
    */
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;

    virtual bool CanInteractWhileCarrying_Implementation(AActor* Interactor) const override;

    virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;

    virtual void Interact_Implementation(AActor* Interactor) override;

    /*
    음식 보관
    */
    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    int32 TryAddFood(
        AFoodItemActor* Food,
        FGuid ExpectedLeaseId);

    UFUNCTION(BlueprintPure, Category = "Food|Buffet")
    bool HasFood() const;

    UFUNCTION(BlueprintPure, Category = "Food|Buffet")
    int32 GetRemainingServings() const;

    UFUNCTION(BlueprintPure, Category = "Food|Buffet")
    UFoodDefinition* GetFoodDefinition() const;

    /*
    손님 대기열
    */
    bool TryJoinQueue(AActor* Requester, FGuid RequestId);

    AActor* GetQueuePoint(AActor* Requester, FGuid RequestId);

    void LeaveQueue(AActor* Requester, FGuid RequestId);

    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    bool TryReserveServingPoint(AActor* Requester, FGuid RequestId);

    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    void ReleaseServingPoint(AActor* Requester, FGuid RequestId);

    /*
    손님 음식 전달
    */
    UFUNCTION(BlueprintCallable, Category = "Food|Buffet")
    AFoodPlateActor* TryTakeServing(
        AActor* Requester,
        FGuid RequestId,
        const FTransform& PlateTransform);

    FRotator GetServingRotation() const;

protected:
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Food|Queue")
    TArray<TObjectPtr<ATargetPoint>> QueuePoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> StationMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> ArrivalArea;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UArrowComponent> ServingDirection;

    // 각 슬롯에 음식 한 개를 보관한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Food|Buffet")
    TArray<TObjectPtr<UFoodSnapSlotComponent>> FoodSlots;

    // 접시 메시가 설정된 FoodPlateActor BP를 지정한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food|Buffet")
    TSubclassOf<AFoodPlateActor> GuestPlateClass;

private:
    struct FQueueEntry
    {
        TWeakObjectPtr<AActor> Guest;
        FGuid VisitId;
    };

    TArray<FQueueEntry> WaitingQueue;

    void RemoveInvalidQueueGuests();

    bool OwnsReservation(AActor* Requester, FGuid RequestId) const;

    UFoodSnapSlotComponent* FindEmptySlot( const AFoodItemActor* Food) const;

    UFoodSnapSlotComponent* FindOccupiedSlot() const;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> ReservedGuest;

    FGuid ReservationId;

    bool bUpdatingStock = false;
};