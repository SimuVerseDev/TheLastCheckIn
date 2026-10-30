#include "GuestCharacter.h"
#include "GuestAIController.h"
#include "Data/GuestDataTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Pool/GuestPoolManager.h"
#include "Guests/Party/GuestParty.h"

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
		GuestState = EGuestState::WaitingForCheckIn;
		break;

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

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	GetMesh()->SetComponentTickEnabled(true);

	GetCharacterMovement()->SetComponentTickEnabled(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	GuestController->SetActorTickEnabled(true);

	if (!GuestController->StartGuestAI(CounterTarget))
	{
		// 입장 시작에 실패하면 다시 풀 대기로 복구
		GuestState = EGuestState::Inactive;
		DeactivateToPool();
		return false;
	}

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