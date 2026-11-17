#include "Guests/Anomaly/Behaviors/GuestNoiseBehavior.h"

#include "Guests/Anomaly/GuestActionLocation.h"
#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/GuestCrowdFollowingComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

bool UGuestNoiseBehavior::CanStart() const
{
    if (!Super::CanStart())
    {
        return false;
    }

    const AGuestActionPoint* Candidate =
        Cast<AGuestActionPoint>(ContextActor.Get());

    return IsValid(Candidate) && Candidate->IsAvailable();
}

void UGuestNoiseBehavior::OnStarted_Implementation()
{
    Super::OnStarted_Implementation();

    if (!InitializeMovement())
    {
        RequestCancelled();
        return;
    }

    Point = Cast<AGuestActionPoint>(ContextActor.Get());

    if (!IsValid(Point) || !Point->TryReserve(this))
    {
        RequestCancelled();
        return;
    }

    // 예약 당시 위치와 방향을 행동 기준으로 저장
    ActionTransform = Point->GetActorTransform();

    MoveToActionPoint();
}

void UGuestNoiseBehavior::BeginActing()
{
    if (!IsRunning() || bActing)
    {
        return;
    }

    // 도착 시 예약과 지점 설정이 유효한지 확인
    // 예약 이후 플레이어가 들어온 경우에는 행동을 유지
    if (!HasMovementContext()
        || !IsValid(Point)
        || !Point->CanBeginAction(this))
    {
        RequestCancelled();
        return;
    }

    FVector ArrivalOffset;

    // 이동 성공 응답뿐 아니라 실제 발 위치도 확인
    if (!TryConfirmArrival(ArrivalOffset))
    {
        RequestCancelled();
        return;
    }

    // 첫 지점까지 이동하던 완료 콜백을 해제한다.
    StopPointMovement();
    ApplyActionFacing();

    // 이후에는 구역 안에서 이동 중이어도 빠루 제압을 허용한다.
    bActing = true;

    // 기존 블루프린트에서 소음 몽타주를 재생한다.
    Point->OnActionStarted(Guest.Get());

    // 이벤트 실행 도중 행동이 종료될 수도 있으므로 다시 확인한다.
    if (IsRunning() && bActing
        && IsValid(Point) && Point->bRoamInZone)
    {
        StartRoaming();
    }
}

void UGuestNoiseBehavior::OnStopped_Implementation()
{
    const bool bWasActing = bActing;
    bActing = false;
    
    // 종료 후 새 이동을 예약하지 않도록 타이머를 해제
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RoamTimer);
    }

    ClearMoveRequest();
    StopPointMovement();
    
    // 소음 행동 이전의 속도와 회전 설정으로 복원
    if (bSavedMovement && IsValid(Guest))
    {
        if (UCharacterMovementComponent* Movement =
            Guest->GetCharacterMovement())
        {
            Movement->MaxWalkSpeed = OriginalWalkSpeed;
            Movement->RotationRate = OriginalRotationRate;
            Movement->bOrientRotationToMovement =
                bOriginalOrientToMovement;
            Movement->bUseControllerDesiredRotation =
                bOriginalUseControllerDesiredRotation;
        }

        Guest->bUseControllerRotationYaw =
            bOriginalUseControllerRotationYaw;
    }

    bSavedMovement = false;
    RoamMoveStartSeconds = -1.0;

    if (IsValid(Point))
    {
        if (bWasActing)
        {
            Point->OnActionStopped(Guest.Get());
            
            // 종료 이벤트 처리 후 소음 몽타주를 멈춤
            if (IsValid(Guest) && IsValid(Point)
                && IsValid(Point->ActionMontage))
            {
                Guest->StopAnimMontage(Point->ActionMontage.Get());
            }
        }

        // 종료 이벤트에서 지점이 제거되었을 수도 있으므로 재확인
        if (IsValid(Point))
        {
            Point->Release(this);
        }
    }

    Point = nullptr;
    ActionTransform = FTransform::Identity;
    GuestController = nullptr;

    Super::OnStopped_Implementation();
}

void UGuestNoiseBehavior::StartRoaming()
{
    if (!HasMovementContext() || !IsValid(Point))
    {
        RequestCancelled();
        return;
    }

    UCharacterMovementComponent* Movement =
        Guest->GetCharacterMovement();

    if (!IsValid(Movement))
    {
        RequestCancelled();
        return;
    }

    // 객실 복귀 시 사용할 기존 속도와 회전 설정을 저장한다.
    OriginalWalkSpeed = Movement->MaxWalkSpeed;
    OriginalRotationRate = Movement->RotationRate;
    bOriginalOrientToMovement = Movement->bOrientRotationToMovement;
    bOriginalUseControllerDesiredRotation =
        Movement->bUseControllerDesiredRotation;
    bOriginalUseControllerRotationYaw = Guest->bUseControllerRotationYaw;
    bSavedMovement = true;

    Movement->MaxWalkSpeed = FMath::Max(1.0f, Point->RoamSpeed);
    Movement->bOrientRotationToMovement = true;
    // 기어다니는 동안 방향을 천천히 전환
    Movement->RotationRate.Yaw = 120.0f;
    Movement->bUseControllerDesiredRotation = false;
    Guest->bUseControllerRotationYaw = false;

    // 도착 후 대기 상태였던 군중 처리를 이동 상태로 전환한다.
    if (UGuestCrowdFollowingComponent* Crowd =
        Cast<UGuestCrowdFollowingComponent>(
            GuestController->GetPathFollowingComponent()))
    {
        Crowd->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
        Crowd->SuspendCrowdSteering(false);
        Crowd->SetRequireActualArrival(false);
        Crowd->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
    }

    NextRoamSeconds = GetWorld()->GetTimeSeconds();
    RoamMoveStartSeconds = -1.0;

    GetWorld()->GetTimerManager().SetTimer(
        RoamTimer,
        this,
        &UGuestNoiseBehavior::UpdateRoaming,
        0.2f,
        true);
}

bool UGuestNoiseBehavior::IsPathInsideZone(
    const FNavPathSharedPtr& Path) const
{
    if (!IsValid(Point) || !IsValid(Point->Zone)
        || !Path.IsValid() || !Path->IsValid()
        || Path->IsPartial()
        || Path->GetPathPoints().Num() < 2)
    {
        return false;
    }

    // 목적지뿐 아니라 이동 경로의 모든 지점을 검사한다.
    for (const FNavPathPoint& PathPoint : Path->GetPathPoints())
    {
        if (!Point->Zone->ContainsLocation(PathPoint.Location))
        {
            return false;
        }
    }

    return true;
}

bool UGuestNoiseBehavior::TryStartRoamMove()
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* Navigation =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    if (!Navigation || !IsValid(Point)
        || !IsValid(Point->Zone)
        || !IsValid(Point->Zone->Bounds))
    {
        return false;
    }

    UBoxComponent* Bounds = Point->Zone->Bounds;
    const FVector Extent = Bounds->GetScaledBoxExtent();

    // 목적지가 구역의 가장자리에 너무 붙지 않도록 여유를 둔다.
    const float Margin =
        Guest->GetCapsuleComponent()->GetScaledCapsuleRadius() + 60.0f;

    const float RangeX = Extent.X - Margin;
    const float RangeY = Extent.Y - Margin;

    if (RangeX <= 0.0f || RangeY <= 0.0f)
    {
        return false;
    }

    const FVector Start = Guest->GetNavAgentLocation();

    // 한 번에 최대 10개의 후보를 검사한다.
    for (int32 Attempt = 0; Attempt < 10; ++Attempt)
    {
        const FVector LocalOffset(
            FMath::FRandRange(-RangeX, RangeX),
            FMath::FRandRange(-RangeY, RangeY),
            0.0f);

        // Extent에 크기 배율이 포함되어 있으므로 중복 적용하지 않는다.
        const FVector Sample =
            Bounds->GetComponentTransform()
                .TransformPositionNoScale(LocalOffset);

        FNavLocation Projected;
        if (!Navigation->ProjectPointToNavigation(
            Sample,
            Projected,
            FVector(50.0f, 50.0f, Extent.Z + 100.0f)))
        {
            continue;
        }

        const FVector Destination = Projected.Location;
        
        // 내비게이션에 투영된 뒤에도 가장자리 여유가 유지되는지 확인
        const FVector LocalDestination =
            Bounds->GetComponentTransform()
                .InverseTransformPositionNoScale(Destination);

        if (FMath::Abs(LocalDestination.X) > RangeX
            || FMath::Abs(LocalDestination.Y) > RangeY)
        {
            continue;
        }

        // 구역 밖이거나 현재 위치와 너무 가까운 후보는 제외한다.
        if (!Point->Zone->ContainsLocation(Destination)
            || FVector::DistSquared2D(Start, Destination)
                < FMath::Square(100.0f))
        {
            continue;
        }

        UNavigationPath* PathObject =
            UNavigationSystemV1::FindPathToLocationSynchronously(
                World,
                Start,
                Destination,
                GuestController.Get(),
                GuestController->GetDefaultNavigationFilterClass());

        if (!IsValid(PathObject))
        {
            continue;
        }

        const FNavPathSharedPtr Path = PathObject->GetPath();

        if (!IsPathInsideZone(Path))
        {
            continue;
        }

        // 자동 재계산으로 검사하지 않은 경로를 사용하지 않도록 한다.
        // 경로가 무효화되면 UpdateRoaming에서 다시 선택한다.
        Path->EnableRecalculationOnInvalidation(false);

        FAIMoveRequest Request;
        Request.SetGoalLocation(Destination);
        Request.SetAcceptanceRadius(40.0f);
        Request.SetReachTestIncludesAgentRadius(false);
        Request.SetReachTestIncludesGoalRadius(false);
        Request.SetAllowPartialPath(false);
        Request.SetProjectGoalLocation(false);
        Request.SetNavigationFilter(
            GuestController->GetDefaultNavigationFilterClass());

        // 위에서 검사한 경로를 그대로 사용한다.
        const FAIRequestID MoveId =
            GuestController->RequestMove(Request, Path);

        if (MoveId != FAIRequestID::InvalidRequest)
        {
            RoamMoveStartSeconds = World->GetTimeSeconds();
            return true;
        }
    }

    return false;
}

void UGuestNoiseBehavior::UpdateRoaming()
{
    if (!IsRunning() || !bActing)
    {
        return;
    }

    if (!HasMovementContext()
        || !IsValid(Point)
        || !Point->CanBeginAction(this))
    {
        RequestCancelled();
        return;
    }

    // 다른 손님에게 밀리는 등 실제 위치가 구역 밖으로 나가면 취소한다.
    if (!Point->Zone->ContainsLocation(Guest->GetNavAgentLocation()))
    {
        RequestCancelled();
        return;
    }

    UPathFollowingComponent* Following =
        GuestController->GetPathFollowingComponent();

    if (!IsValid(Following))
    {
        RequestCancelled();
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();

    if (Following->GetStatus() == EPathFollowingStatus::Moving)
    {
        const bool bTimedOut =
            RoamMoveStartSeconds >= 0.0
            && Now - RoamMoveStartSeconds >= 30.0;

        // 오래 막히거나 경로가 무효화되면 멈추고 다시 선택한다.
        if (bTimedOut || !IsPathInsideZone(Following->GetPath()))
        {
            GuestController->StopMovement();
            RoamMoveStartSeconds = -1.0;
            NextRoamSeconds = Now + 1.0;
        }

        return;
    }

    if (Following->GetStatus() != EPathFollowingStatus::Idle)
    {
        GuestController->StopMovement();
        RoamMoveStartSeconds = -1.0;
        NextRoamSeconds = Now + 1.0;
        return;
    }

    if (Now < NextRoamSeconds)
    {
        return;
    }

    // 후보를 찾지 못해도 소음 행동은 유지하고 1초 뒤 다시 시도한다.
    NextRoamSeconds = Now + 1.0;
    TryStartRoamMove();
}
