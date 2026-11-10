#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GuestAnomalyComponent.generated.h"

class UGuestAnomalyBehavior;
class UGuestAnomalyDefinition;
class UGuestAnomalyComponent;

UENUM(BlueprintType)
enum class EGuestAnomalyPhase : uint8
{
    Idle, //사건에 참여하지 않음
    Active, //이상행동 진행 중
    Resolved // 어떤 손님은 제압됐지만 사건 정리는 아직 진행 중
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGuestAnomalyEvent,
    UGuestAnomalyComponent*, Component);

UCLASS(
    ClassGroup = (Guests),
    meta = (BlueprintSpawnableComponent))
class THELASTCHECKIN_API UGuestAnomalyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGuestAnomalyComponent();

    UFUNCTION(BlueprintCallable, Category = "Anomaly")
    bool StartAnomaly(
        UGuestAnomalyDefinition* InDefinition,
        AActor* ContextActor);

    UFUNCTION(BlueprintCallable, Category = "Anomaly")
    void SendSignal(
        FName Signal,
        AActor* InstigatorActor);

    // 해결 이후 사건 정리까지 끝났을 때 호출
    UFUNCTION(BlueprintCallable, Category = "Anomaly")
    bool FinishIncident();

    UFUNCTION(BlueprintCallable, Category = "Anomaly")
    void CancelAnomaly();

    UFUNCTION(BlueprintPure, Category = "Anomaly")
    bool IsBusy() const
    {
        return Phase != EGuestAnomalyPhase::Idle;
    }

    UFUNCTION(BlueprintPure, Category = "Anomaly")
    EGuestAnomalyPhase GetPhase() const
    {
        return Phase;
    }

    UPROPERTY(BlueprintAssignable, Category = "Anomaly")
    FGuestAnomalyEvent OnResolved;

    UPROPERTY(BlueprintAssignable, Category = "Anomaly")
    FGuestAnomalyEvent OnCancelled;

    void ResolveBehavior(UGuestAnomalyBehavior* Requester);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StopGuestMovement();
    void DestroyBehavior();

    UPROPERTY(Transient)
    TObjectPtr<UGuestAnomalyBehavior> ActiveBehavior;

    UPROPERTY(Transient)
    TObjectPtr<UGuestAnomalyDefinition> CurrentDefinition;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Anomaly",
        meta = (AllowPrivateAccess = "true"))
    EGuestAnomalyPhase Phase = EGuestAnomalyPhase::Idle;

    bool bTransitioning = false;
};