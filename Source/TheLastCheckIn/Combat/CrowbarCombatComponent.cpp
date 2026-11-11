#include "Combat/CrowbarCombatComponent.h"

#include "Combat/CrowbarActor.h"
#include "TheLastCheckInCharacter.h"
#include "Core/Interaction/PlayerInteractionComponent.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Anomaly/GuestAnomalyComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UCrowbarCombatComponent::UCrowbarCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UCrowbarCombatComponent::IsEquipped() const
{
    return EquippedCrowbar.IsValid();
}

ACrowbarActor* UCrowbarCombatComponent::GetEquippedCrowbar() const
{
    return EquippedCrowbar.Get();
}

bool UCrowbarCombatComponent::CanEquip() const
{
    const ATheLastCheckInCharacter* Player =
        Cast<ATheLastCheckInCharacter>(GetOwner());

    if (!IsValid(Player) ||
        !IsValid(Player->GetFirstPersonMesh()) ||
        IsEquipped())
    {
        return false;
    }

    UPlayerInteractionComponent* Interaction =
        Player->FindComponentByClass<UPlayerInteractionComponent>();

    if (!IsValid(Interaction) || Interaction->HasActiveInteraction())
    {
        return false;
    }

    // 다른 작업 중이면 장착 불가
    for (UActorComponent* Component : Player->GetComponents())
    {
        if (const IInteractionExtension* Extension =
            Cast<IInteractionExtension>(Component))
        {
            if (Extension->BlocksFocus())
            {
                return false;
            }
        }
    }

    return true;
}

bool UCrowbarCombatComponent::TryEquip(ACrowbarActor* Crowbar)
{
    if (!CanEquip() ||
        !IsValid(Crowbar) ||
        Crowbar->GetWorld() != GetWorld() ||
        Crowbar->GetOwner() != nullptr ||
        !IsValid(Crowbar->Mesh) ||
        !Crowbar->Mesh->GetStaticMesh())
    {
        return false;
    }

    ATheLastCheckInCharacter* Player =
        Cast<ATheLastCheckInCharacter>(GetOwner());

    USkeletalMeshComponent* HandMesh =
        Player->GetFirstPersonMesh();

    if (!Crowbar->AttachSocket.IsNone() &&
        !HandMesh->DoesSocketExist(Crowbar->AttachSocket))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Crowbar socket not found: %s"),
            *Crowbar->AttachSocket.ToString());

        return false;
    }

    const bool bWasSimulating = Crowbar->Mesh->IsSimulatingPhysics();

    Crowbar->Mesh->SetSimulatePhysics(false);

    if (!Crowbar->AttachToComponent(
        HandMesh,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        Crowbar->AttachSocket))
    {
        Crowbar->Mesh->SetSimulatePhysics(bWasSimulating);
        return false;
    }

    EquippedCrowbar = Crowbar;

    Crowbar->SetOwner(Player);
    Crowbar->SetActorRelativeTransform(Crowbar->HandTransform);
    Crowbar->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Crowbar->Mesh->SetOnlyOwnerSee(true);
    Crowbar->Mesh->FirstPersonPrimitiveType =
        EFirstPersonPrimitiveType::FirstPerson;

    return true;
}

void UCrowbarCombatComponent::DropCrowbar(bool bEnablePhysics)
{
    ACrowbarActor* Crowbar = EquippedCrowbar.Get();

    EquippedCrowbar.Reset();
    CancelPendingHit();

    if (!IsValid(Crowbar))
    {
        return;
    }

    Crowbar->DetachFromActor(
        FDetachmentTransformRules::KeepWorldTransform);

    Crowbar->SetOwner(nullptr);

    Crowbar->Mesh->SetOnlyOwnerSee(false);
    Crowbar->Mesh->FirstPersonPrimitiveType =
        EFirstPersonPrimitiveType::None;

    Crowbar->Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Crowbar->Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Crowbar->Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    Crowbar->Mesh->SetSimulatePhysics(bEnablePhysics);

    if (bEnablePhysics)
    {
        Crowbar->Mesh->WakeAllRigidBodies();
    }
}

void UCrowbarCombatComponent::NotifyCrowbarRemoved(
    ACrowbarActor* Crowbar)
{
    if (!EquippedCrowbar.IsValid() ||
        EquippedCrowbar.Get() == Crowbar)
    {
        EquippedCrowbar.Reset();
        CancelPendingHit();
    }
}

bool UCrowbarCombatComponent::TryHandleUseStarted()
{
    if (!IsEquipped())
    {
        return false;
    }

    TryAttack();

    // 쿨타임 중에도 입력은 소비
    return true;
}

void UCrowbarCombatComponent::GetIgnoredActors(
    TArray<AActor*>& IgnoredActors) const
{
    if (ACrowbarActor* Crowbar = EquippedCrowbar.Get())
    {
        IgnoredActors.AddUnique(Crowbar);
    }
}

void UCrowbarCombatComponent::TryAttack()
{
    UWorld* World = GetWorld();
    APawn* Pawn = Cast<APawn>(GetOwner());

    if (!World ||
        !IsValid(Pawn) ||
        !IsValid(Pawn->GetController()) ||
        !IsEquipped() ||
        bHitPending)
    {
        return;
    }

    const double Now = World->GetTimeSeconds();

    if (Now < NextAttackTime)
    {
        return;
    }

    const float Cooldown = FMath::Max(0.1f, AttackCooldown);
    const float Delay = FMath::Clamp(HitDelay, 0.0f, Cooldown);

    NextAttackTime = Now + Cooldown;
    bHitPending = true;

    OnAttackStarted.Broadcast();

    if (!IsEquipped() || !bHitPending)
    {
        return;
    }

    if (Delay <= 0.0f)
    {
        PerformHit();
    }
    else
    {
        World->GetTimerManager().SetTimer(
            HitTimer,
            this,
            &UCrowbarCombatComponent::PerformHit,
            Delay,
            false);
    }
}

void UCrowbarCombatComponent::PerformHit()
{
    if (!bHitPending)
    {
        return;
    }

    bHitPending = false;

    UWorld* World = GetWorld();
    APawn* Pawn = Cast<APawn>(GetOwner());

    if (!World ||
        !IsEquipped() ||
        !IsValid(Pawn) ||
        !IsValid(Pawn->GetController()))
    {
        return;
    }

    FVector Start;
    FRotator Rotation;

    Pawn->GetActorEyesViewPoint(Start, Rotation);

    if (ATheLastCheckInCharacter* Player =
        Cast<ATheLastCheckInCharacter>(Pawn))
    {
        UCameraComponent* Camera =
            Player->GetFirstPersonCameraComponent();

        if (IsValid(Camera))
        {
            Start = Camera->GetComponentLocation();
            Rotation = Camera->GetComponentRotation();
        }
    }

    const FVector End =
        Start + Rotation.Vector() * FMath::Max(1.0f, AttackRange);

    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(CrowbarHit),
        false,
        Pawn);

    // 들고 있는 빠루는 제외
    if (ACrowbarActor* Crowbar = EquippedCrowbar.Get())
    {
        Params.AddIgnoredActor(Crowbar);
    }

    FHitResult Hit;

    const bool bHit = World->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        HitChannel,
        FCollisionShape::MakeSphere(FMath::Max(1.0f, HitRadius)),
        Params);

    if (bDrawDebug)
    {
        const FVector DebugEnd = bHit ? Hit.Location : End;
        const FColor Color = bHit ? FColor::Red : FColor::Green;

        DrawDebugLine(
            World,
            Start,
            DebugEnd,
            Color,
            false,
            0.5f);

        DrawDebugSphere(
            World,
            DebugEnd,
            FMath::Max(1.0f, HitRadius),
            12,
            Color,
            false,
            0.5f);
    }

    AGuestCharacter* Guest =
        bHit ? Cast<AGuestCharacter>(Hit.GetActor()) : nullptr;

    if (!IsValid(Guest))
    {
        return;
    }

    UGuestAnomalyComponent* Anomaly =
        Guest->GetAnomalyComponent();

    if (IsValid(Anomaly))
    {
        Anomaly->SendSignal(TEXT("Crowbar"), Pawn);
    }
}

void UCrowbarCombatComponent::CancelPendingHit()
{
    bHitPending = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitTimer);
    }
}

void UCrowbarCombatComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    DropCrowbar(EndPlayReason == EEndPlayReason::Destroyed);

    Super::EndPlay(EndPlayReason);
}