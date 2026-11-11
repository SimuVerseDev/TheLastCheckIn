#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Interaction/InteractionExtension.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "CrowbarCombatComponent.generated.h"

class ACrowbarActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCrowbarAttackEvent);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class THELASTCHECKIN_API UCrowbarCombatComponent
    : public UActorComponent
    , public IInteractionExtension
{
    GENERATED_BODY()

public:
    UCrowbarCombatComponent();

    bool CanEquip() const;
    bool TryEquip(ACrowbarActor* Crowbar);

    void DropCrowbar(bool bEnablePhysics = true);
    void NotifyCrowbarRemoved(ACrowbarActor* Crowbar);

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsEquipped() const;

    ACrowbarActor* GetEquippedCrowbar() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void TryAttack();

    virtual bool TryHandleUseStarted() override;

    virtual void GetIgnoredActors(
        TArray<AActor*>& IgnoredActors) const override;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FCrowbarAttackEvent OnAttackStarted;

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    // 공격 거리
    UPROPERTY(EditAnywhere, Category = "Combat",
        meta = (ClampMin = "1"))
    float AttackRange = 180.0f;

    // 판정 반경
    UPROPERTY(EditAnywhere, Category = "Combat",
        meta = (ClampMin = "1"))
    float HitRadius = 15.0f;

    // 공격 간격
    UPROPERTY(EditAnywhere, Category = "Combat",
        meta = (ClampMin = "0.1"))
    float AttackCooldown = 0.6f;

    // 타격 시점
    UPROPERTY(EditAnywhere, Category = "Combat",
        meta = (ClampMin = "0"))
    float HitDelay = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Combat")
    TEnumAsByte<ECollisionChannel> HitChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, Category = "Combat|Debug")
    bool bDrawDebug = false;

private:
    void PerformHit();
    void CancelPendingHit();

    TWeakObjectPtr<ACrowbarActor> EquippedCrowbar;

    FTimerHandle HitTimer;

    double NextAttackTime = 0.0;
    bool bHitPending = false;
};