#include "Guests/Anomaly/GuestActionLocation.h"

#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

AGuestActionZone::AGuestActionZone()
{
    PrimaryActorTick.bCanEverTick = false;

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);

    Bounds->SetBoxExtent(FVector(500.0, 250.0, 200.0));

    // 겹침 이벤트가 아닌 좌표 검사로 구역을 판정
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetCanEverAffectNavigation(false);
}

bool AGuestActionZone::ContainsLocation(FVector Location) const
{
    if (!IsValid(Bounds) || Location.ContainsNaN())
    {
        return false;
    }

    // 회전하거나 크기가 변경된 박스도 로컬 좌표로 검사
    const FVector LocalLocation =
        Bounds->GetComponentTransform()
        .InverseTransformPosition(Location);

    const FVector Extent = Bounds->GetUnscaledBoxExtent();

    return FBox(-Extent, Extent).IsInsideOrOn(LocalLocation);
}

bool AGuestActionZone::CanStartAction() const
{
    if (!bEnabled || IsActorBeingDestroyed() || !GetWorld())
    {
        return false;
    }

    bool bHasPlayer = false;

    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* Controller = It->Get();

        const APawn* Pawn = IsValid(Controller)
            ? Controller->GetPawn()
            : nullptr;

        if (!IsValid(Pawn))
        {
            continue;
        }

        bHasPlayer = true;

        if (ContainsLocation(Pawn->GetActorLocation()))
        {
            return false;
        }
    }

    // 플레이어 생성 전에는 사건을 시작하지 않음
    return bHasPlayer;
}

AGuestActionPoint::AGuestActionPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    Facing = CreateDefaultSubobject<UArrowComponent>(TEXT("Facing"));
    SetRootComponent(Facing);

    Facing->SetCanEverAffectNavigation(false);
}

bool AGuestActionPoint::HasValidPlacement() const
{
    return bEnabled
        && !IsActorBeingDestroyed()
        && IsValid(Zone)
        && !Zone->IsActorBeingDestroyed()
        && Zone->bEnabled
        && Zone->GetWorld() == GetWorld()
        && Zone->ContainsLocation(GetActorLocation());
}

bool AGuestActionPoint::IsAvailable() const
{
    // 지점을 선택할 때는 플레이어가 없는 구역만 허용
    return !Reservation.IsValid()
        && HasValidPlacement()
        && Zone->CanStartAction();
}

bool AGuestActionPoint::TryReserve(UObject* Requester)
{
    // 실제 예약 직전에도 플레이어 유무와 사용 가능 여부를 확인
    if (!IsValid(Requester) || !IsAvailable())
    {
        return false;
    }

    Reservation = Requester;
    return true;
}

bool AGuestActionPoint::IsReservedBy(const UObject* Requester) const
{
    return Requester && Reservation.Get() == Requester;
}

bool AGuestActionPoint::CanBeginAction(const UObject* Requester) const
{
    // 예약 이후 플레이어가 들어와도 행동을 유지
    // 예약과 지점 설정이 여전히 유효한지만 확인
    return IsReservedBy(Requester) && HasValidPlacement();
}

void AGuestActionPoint::Release(UObject* Requester)
{
    if (IsReservedBy(Requester))
    {
        Reservation.Reset();
    }
}