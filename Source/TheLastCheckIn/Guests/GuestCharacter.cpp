#include "GuestCharacter.h"
#include "GuestAIController.h"
#include "Data/GuestDataTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Pool/GuestPoolManager.h"
#include "Guests/Party/GuestParty.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "CheckIn/CheckInCounter.h"

AGuestCharacter::AGuestCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AIControllerClass = AGuestAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}


bool AGuestCharacter::NotifyDestinationReached(AActor* ReachedTarget)
{
	if (!IsValid(ReachedTarget) ||
		ReachedTarget != MoveTarget.Get())
	{
		return false;
	}

	switch (GuestState)
	{
	case EGuestState::MovingToCounter:
		{
			const ACheckInCounter* Counter = Cast<ACheckInCounter>(ReachedTarget);

			FTransform Slot;

			if (!IsValid(Counter) ||
				!Counter->GetSlotTransformForGuest(this, Slot))
			{
				return false;
			}

			const FRotator FacingRotation(
				0.0f,
				Slot.Rotator().Yaw,
				0.0f);

			SetActorRotation(FacingRotation);

			if (AController* GuestController = GetController())
			{
				GuestController->SetControlRotation(FacingRotation);
			}

			GuestState = EGuestState::WaitingForCheckIn;
			break;
		}

	case EGuestState::MovingToRoom:
		GuestState = EGuestState::Staying;
		break;

	case EGuestState::MovingToLobby:
		GuestState = EGuestState::ReadyToExit;
		break;

	case EGuestState::MovingToExit:
		GuestState = EGuestState::ExitCompleted;

		if (IsValid(PoolManager.Get()))
		{
			PoolReturnTimerHandle =
				GetWorldTimerManager().SetTimerForNextTick(
					this,
					&AGuestCharacter::ReturnToPool);
		}
		break;

	default:
		return false;
	}
	
	GetCharacterMovement()->StopMovementImmediately();

	if (AGuestAIController* GuestController =Cast<AGuestAIController>(GetController()))
	{
		GuestController->UseWaitingAvoidance();
	}


	MoveTarget = nullptr;
	return true;
}

bool AGuestCharacter::ActivateFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation,
AActor* CounterTarget, const FDataTableRowHandle& InGuestTypeRow)
{
	// 대기 중인 손님만 꺼낼 수 있음
	if (GuestState != EGuestState::Inactive ||
		Party != nullptr ||
		!IsValid(CounterTarget))
	{
		return false;
	}

	AGuestAIController* GuestController =
		Cast<AGuestAIController>(GetController());

	if (!IsValid(GuestController))
	{
		return false;
	}

	// 새 손님의 유형과 이동 속도 적용
	if (!ApplyGuestData(InGuestTypeRow))
	{
		return false;
	}

	// 아직 충돌이 꺼진 상태에서 입장 위치로 이동
	if (!SetActorLocationAndRotation(
		SpawnLocation,
		SpawnRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics))
	{
		DeactivateToPool();
		return false;
	}
	

	// 카운터 도착 판정에 사용하는 목적지도 갱신
	InitialMoveTarget = CounterTarget;
	MoveTarget = CounterTarget;
	GuestState = EGuestState::MovingToCounter;
	
	// 아직 숨겨진 상태에서 새 외형 적용
	OnPrepareGuestAppearance();

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	GetMesh()->SetComponentTickEnabled(true);
	
	// BP에서 추가할 외형 컴포넌트도 활성화
	OnGuestAppearanceActiveChanged(true);

	GetCharacterMovement()->SetComponentTickEnabled(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	GuestController->SetActorTickEnabled(true);
	

	return true;
	
}

bool AGuestCharacter::DeactivateToPool()
{
	// 최초 풀 대기 또는 정상 퇴실 완료 상태에서만 허용
	if (GuestState != EGuestState::Inactive &&
		GuestState != EGuestState::ExitCompleted)
	{
		return false;
	}

	// 객실과 연결된 손님은 반환하지 않음
	if (IsValid(Party.Get()) && Party->HasAssignedRoom())
	{
		return false;
	}
	
	ReleaseMoveLocation();
	
	GetWorldTimerManager().ClearTimer(PoolReturnTimerHandle);
	GuestState = EGuestState::Inactive;

	AGuestAIController* GuestController =
		Cast<AGuestAIController>(GetController());

	if (IsValid(GuestController))
	{
		GuestController->StopGuestAI();
		GuestController->SetActorTickEnabled(false);
	}

	// 이동과 애니메이션 업데이트 정지
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);

	GetMesh()->SetComponentTickEnabled(false);
	
	//BP에서 추가할 외형 컴포넌트도 비활성화
	OnGuestAppearanceActiveChanged(false);

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	// 이전 일행 정보 초기화
	Party = nullptr;
	InitialMoveTarget = nullptr;
	MoveTarget = nullptr;
	GuestTypeRow = FDataTableRowHandle{};

	return true;
}

void AGuestCharacter::SetPoolManager(AGuestPoolManager* InPoolManager)
{
	PoolManager = InPoolManager;
}

bool AGuestCharacter::CanInteract_Implementation(AActor* Interactor) const
{
	return IsValid(Interactor)
	&& GuestState == EGuestState::WaitingForCheckoutRequest
	&& IsValid(Party.Get())
	&& Party->CanRequestCheckout();
}

FText AGuestCharacter::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return NSLOCTEXT("GuestInteraction","RequestCheckout","퇴실 요청");
}

void AGuestCharacter::Interact_Implementation(AActor* Interactor)
{
	// 실행 순간에도 조건 재확인
	if (!IInteractable::Execute_CanInteract(this, Interactor))
	{
		return;
	}

	Party->RequestCheckout();
}

void AGuestCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (bStartInPool)
	{
		GuestState = EGuestState::Inactive;
		DeactivateToPool();
		return;
	}
	
	ApplyGuestData(GuestTypeRow);
	
}

void AGuestCharacter::ReturnToPool()
{
	PoolReturnTimerHandle.Invalidate();

	if (GuestState != EGuestState::ExitCompleted ||
		!IsValid(PoolManager.Get()))
	{
		return;
	}

	PoolManager->ReleaseGuest(this);
}

bool AGuestCharacter::ApplyGuestData(const FDataTableRowHandle& InGuestTypeRow)
{
	if (InGuestTypeRow.IsNull())
	{
		return false;
	}

	const FGuestTypeRow* GuestData =
		InGuestTypeRow.GetRow<FGuestTypeRow>(
			TEXT("AGuestCharacter::ApplyGuestData"));

	if (!GuestData)
	{
		return false;
	}

	GuestTypeRow = InGuestTypeRow;

	GetCharacterMovement()->MaxWalkSpeed =
		FMath::Max(0.0f, GuestData->WalkSpeed);

	return true;
}




bool AGuestCharacter::CancelPoolActivation()
{
	// 이미 객실을 배정받았다면 취소할 수 없음
	if (IsValid(Party.Get()) && Party->HasAssignedRoom())
	{
		return false;
	}

	if (GuestState != EGuestState::MovingToCounter &&
		GuestState != EGuestState::WaitingForCheckIn)
	{
		return false;
	}

	GuestState = EGuestState::Inactive;
	
	return DeactivateToPool();
}

UGuestParty* AGuestCharacter::GetParty() const
{
	return Party.Get();
}

void AGuestCharacter::AbortVisitAndReturnToPool()
{
	if (IsValid(Party.Get()) && Party->HasAssignedRoom())
	{
		return;
	}

	// 정상 도착과 동일한 풀 정리 경로 사용. DeactivateToPool이 AI와 타이머도 정리.
	GuestState = EGuestState::ExitCompleted;
	if (IsValid(PoolManager.Get()) && PoolManager->ReleaseGuest(this))
	{
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("Guest %s: Pool return failed during visit cancellation; deactivating."), *GetName());
	DeactivateToPool();
}

bool AGuestCharacter::BeginMoveTo(
	AActor* Target,
	EGuestState MovingState)
{
	if (!IsValid(Target))
	{
		return false;
	}

	AGuestAIController* GuestController =
		Cast<AGuestAIController>(GetController());

	if (!IsValid(GuestController))
	{
		return false;
	}

	const EGuestState PreviousState = GuestState;
	AActor* PreviousTarget = MoveTarget.Get();

	GuestState = MovingState;
	MoveTarget = Target;

	// 이전 이동을 정리하고 새 목적지로 AI 시작
	if (!GuestController->StartGuestAI(Target))
	{
		GuestState = PreviousState;
		MoveTarget = PreviousTarget;
		return false;
	}

	return true;
}

bool AGuestCharacter::CanReserveMoveLocation(const FVector& Location) const
{
    UWorld* World = GetWorld();
    AActor* Destination = MoveTarget.Get();

    if (!World || !IsValid(Destination) || Location.ContainsNaN())
    {
        return false;
    }

    // 목적지에 지정된 도착 가능 영역 찾기
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	if (const ACheckInCounter* Counter =
		Cast<ACheckInCounter>(Destination))
	{
		FTransform Slot;

		if (!Counter->GetSlotTransformForGuest(this, Slot))
		{
			return false;
		}

		// 이 손님에게 배정된 고정 자리인지 확인.
		if (FVector::DistSquared2D(
				Location, Slot.GetLocation()) > FMath::Square(10.0) ||
			FMath::Abs(Location.Z - Slot.GetLocation().Z) > 200.0)
		{
			return false;
		}
	}
	else
	{
		// 객실·로비 등은 기존 EQS 도착 영역 검사.
		UBoxComponent* ArrivalArea = nullptr;
		TInlineComponentArray<UBoxComponent*> Boxes(Destination);

		for (UBoxComponent* Box : Boxes)
		{
			if (Box->ComponentHasTag(TEXT("GuestArrivalArea")))
			{
				ArrivalArea = Box;
				break;
			}
		}

		if (!IsValid(ArrivalArea))
		{
			return false;
		}

		const FTransform AreaTransform =
			ArrivalArea->GetComponentTransform();

		const FVector LocalPoint =
			AreaTransform.InverseTransformPosition(Location);

		const FVector Extent = ArrivalArea->GetUnscaledBoxExtent();
		const FVector Scale = AreaTransform.GetScale3D();

		const double PaddingX =
			Radius / FMath::Max(FMath::Abs(Scale.X), 0.001);

		const double PaddingY =
			Radius / FMath::Max(FMath::Abs(Scale.Y), 0.001);

		if (FMath::Abs(LocalPoint.X) + PaddingX > Extent.X ||
			FMath::Abs(LocalPoint.Y) + PaddingY > Extent.Y ||
			FMath::Abs(LocalPoint.Z) > Extent.Z)
		{
			return false;
		}
	}

    // 후보 좌표는 바닥 위치이므로 캡슐 중심 높이로 보정
    const FVector CapsuleCenter =
        Location + FVector(0.0, 0.0, HalfHeight + 5.0f);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // 벽, 가구, 현재 서 있는 손님과 충돌하면 제외
    if (World->OverlapBlockingTestByProfile(
            CapsuleCenter,
            FQuat::Identity,
            Capsule->GetCollisionProfileName(),
            FCollisionShape::MakeCapsule(Radius, HalfHeight),
            Params))
    {
        return false;
    }

    // 다른 일행까지 포함하여 예약된 자리 검사
    for (TActorIterator<AGuestCharacter> It(World); It; ++It)
    {
        const AGuestCharacter* Other = *It;

        if (!IsValid(Other) ||
            Other == this ||
            !Other->bHasReservedMoveLocation)
        {
            continue;
        }

        const UCapsuleComponent* OtherCapsule =
            Other->GetCapsuleComponent();

        const float OtherRadius =
            OtherCapsule->GetScaledCapsuleRadius();

        const float OtherHalfHeight =
            OtherCapsule->GetScaledCapsuleHalfHeight();

        const FVector OtherCenter =
            Other->ReservedMoveLocation +
            FVector(0.0, 0.0, OtherHalfHeight + 5.0f);

        // 서로 다른 층의 자리는 간섭하지 않음
        const bool bHeightOverlaps =
            FMath::Abs(CapsuleCenter.Z - OtherCenter.Z) <
            HalfHeight + OtherHalfHeight;

        // 도착 허용 반경까지 고려해서 자리 간격 확보
        const float RequiredDistance =
            Radius + OtherRadius +
            MoveAcceptanceRadius * 2.0f + 10.0f;

        if (bHeightOverlaps &&
            FVector::DistSquared2D(
                Location, Other->ReservedMoveLocation) <
                FMath::Square(RequiredDistance))
        {
            return false;
        }
    }

    return true;
}

bool AGuestCharacter::TryReserveMoveLocation(
    const FVector& Location)
{
    // EQS 계산 이후 다른 손님이 예약했을 수 있으므로 재검사
    if (!CanReserveMoveLocation(Location))
    {
        return false;
    }

    ReservedMoveLocation = Location;
    bHasReservedMoveLocation = true;
    return true;
}

void AGuestCharacter::ReleaseMoveLocation()
{
    bHasReservedMoveLocation = false;
    ReservedMoveLocation = FVector::ZeroVector;
}