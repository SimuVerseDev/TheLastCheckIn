#include "Guests/Anomaly/Behaviors/GuestNoiseBehavior.h"

#include "Guests/Anomaly/GuestActionLocation.h"
#include "Guests/GuestCharacter.h"

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

    ApplyActionFacing();

    // 실제 도착과 방향 적용이 끝난 뒤부터 빠루 제압 허용
    bActing = true;

    // 애니메이션과 소리는 나중에 이 이벤트에 연결
    Point->OnActionStarted(Guest.Get());
}

void UGuestNoiseBehavior::OnStopped_Implementation()
{
    const bool bWasActing = bActing;
    bActing = false;

    ClearMoveRequest();
    StopPointMovement();

    if (IsValid(Point))
    {
        if (bWasActing)
        {
            Point->OnActionStopped(Guest.Get());
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