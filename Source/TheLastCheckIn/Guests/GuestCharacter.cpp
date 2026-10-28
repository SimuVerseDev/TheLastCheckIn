#include "GuestCharacter.h"
#include "GuestAIController.h"
#include "Data/GuestDataTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rooms/RoomActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Pool/GuestPoolManager.h"

AGuestCharacter::AGuestCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AIControllerClass = AGuestAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}


bool AGuestCharacter::TryAssignRoom(ARoomActor* NewRoom)
{
	if (GuestState!=EGuestState::WaitingForCheckIn)
	{
		return false;
	}
	
	if (!IsValid(NewRoom))
	{
		return false;
	}
	
	//중복 배정 방지
	if (AssignedRoom != nullptr || StayId.IsValid())
	{
		return false;
	}
	
	AGuestAIController* GuestController = 
		Cast<AGuestAIController>(GetController());
	
	if (!IsValid(GuestController))
	{
		return false;
	}
	
	AActor* RoomTarget = NewRoom->GetArrivalTarget();

	if (!IsValid(RoomTarget))
	{
		return false;
	}
	
	const FGuid NewStayId = FGuid::NewGuid();
	
	if (!NewRoom->TryReserve(NewStayId))
	{
		return false;
	}

	AssignedRoom = NewRoom;
	StayId = NewStayId;
	
	GuestState = EGuestState::MovingToRoom;
	
	if (!GuestController->SetGuestMoveTarget(RoomTarget))
	{
		NewRoom->CancelReservation(NewStayId);

		AssignedRoom = nullptr;
		StayId.Invalidate();
		
		GuestState = EGuestState::WaitingForCheckIn;

		return false;
	}
	
	//객실 배정에 성공하면 이번 투숙의 잔류 여부 결정
	switch (CheckoutPolicy)
	{
	case EGuestCheckoutPolicy::Automatic:
		bWillStayBehind = false;
		break;

	case EGuestCheckoutPolicy::RequireRequest:
		bWillStayBehind = true;
		break;

	case EGuestCheckoutPolicy::Random:
		bWillStayBehind =
			StayBehindChance >= 1.0f ||
			(StayBehindChance > 0.0f &&
			 FMath::FRand() < StayBehindChance);
		break;

	default:
		bWillStayBehind = false;
		break;
	}
	
	// 객실 배정과 퇴실 정책 결정까지 성공한 뒤 알림
	OnGuestRoomAssigned.Broadcast(this);

	return true;

}

bool AGuestCharacter::NotifyDestinationReached(AActor* ReachedTarget)
{
	if (!IsValid(ReachedTarget))
	{
		return false;
	}

	switch (GuestState)
	{
	case EGuestState::MovingToCounter:
	{
		if (ReachedTarget!=InitialMoveTarget.Get())
		{
			return false;
		}
		
		GuestState = EGuestState::WaitingForCheckIn;

		return true;
	}
	
	case EGuestState::MovingToRoom:
		{
			if (!IsValid(AssignedRoom.Get()))
			{
				return false;
			}
			
			if (ReachedTarget != AssignedRoom->GetArrivalTarget())
			{
				return false;
			}

			// 객실의 입실 처리에 성공한 경우만 투숙으로 변경
			if (!AssignedRoom->MarkOccupied(StayId))
			{
				return false;
			}

			GuestState = EGuestState::Staying;

			return true;
		}
	
	case EGuestState::MovingToLobby:
		{
			if (!IsValid(CheckoutLobbyTarget.Get()) ||
				ReachedTarget != CheckoutLobbyTarget.Get())
			{
				return false;
			}
			
			if (!IsValid(AssignedRoom.Get()) || !StayId.IsValid())
			{
				return false;
			}
			
			if (!AssignedRoom->TryEndStay(StayId))
			{
				return false;
			}
			
			AssignedRoom = nullptr;
			CheckoutLobbyTarget = nullptr;
			
			GuestState = EGuestState::ReadyToExit;
			
			return true;
		}
	case EGuestState::MovingToExit:
		{
			if (!IsValid(CheckoutExitTarget.Get())||
				ReachedTarget!=CheckoutExitTarget.Get())
			{
				return false;
			}
			
			CheckoutExitTarget = nullptr;
			GuestState = EGuestState::ExitCompleted;
			
			if (IsValid(PoolManager.Get()))
			{
				PoolReturnTimerHandle = 
					GetWorldTimerManager().SetTimerForNextTick(this, &AGuestCharacter::ReturnToPool);
			}
			
			return true;
		}

	default:
		return false;
	}
}

bool AGuestCharacter::TryBeginCheckout(AActor* LobbyTarget)
{
	if (GuestState!=EGuestState::Staying &&
		GuestState != EGuestState::WaitingForCheckoutRequest)
	{
		return false;
	}
	
	if (!IsValid(AssignedRoom.Get()) ||
		!StayId.IsValid() ||
		!IsValid(LobbyTarget))
	{
		return false;
	}
	
	AGuestAIController* GuestController = Cast<AGuestAIController>(GetController());
	
	if (!IsValid(GuestController))
	{
		return false;
	}
	
	// 이동 요청 실패 시 돌아갈 상태
	const EGuestState PreviousState = GuestState;
	AActor* PreviousLobbyTarget = CheckoutLobbyTarget.Get();

	CheckoutLobbyTarget = LobbyTarget;
	GuestState = EGuestState::MovingToLobby;

	// 로비를 새 이동 목적지로 등록
	if (!GuestController->SetGuestMoveTarget(LobbyTarget))
	{
		CheckoutLobbyTarget = PreviousLobbyTarget;
		GuestState = PreviousState;
		return false;
	}

	return true;
}

bool AGuestCharacter::TryBeginExit(AActor* ExitTarget)
{
	if (GuestState != EGuestState::ReadyToExit)
	{
		return false;
	}
	
	if (!IsValid(ExitTarget) || AssignedRoom != nullptr)
	{
		return false;
	}
	
	AGuestAIController* GuestController = Cast<AGuestAIController>(GetController());
	
	if (!IsValid(GuestController))
	{
		return false;
	}
	
	CheckoutExitTarget = ExitTarget;
	GuestState = EGuestState::MovingToExit;

	if (!GuestController->SetGuestMoveTarget(ExitTarget))
	{
		CheckoutExitTarget = nullptr;
		GuestState = EGuestState::ReadyToExit;

		return false;
	}

	return true;
}

bool AGuestCharacter::HandleCheckoutTime(AActor* LobbyTarget)
{
	if (GuestState != EGuestState::Staying ||
	!IsValid(AssignedRoom.Get()) ||
	!StayId.IsValid() ||
	!IsValid(LobbyTarget))
	{
		return false;
	}

	// 객실 배정 시 결정한 잔류 여부 사용
	if (bWillStayBehind)
	{
		// 플레이어가 나중에 퇴실을 요청할 때 사용할 목적지
		CheckoutLobbyTarget = LobbyTarget;
		
		GuestState = EGuestState::WaitingForCheckoutRequest;
		return true;
	}

	return TryBeginCheckout(LobbyTarget);
	
}

bool AGuestCharacter::RequestCheckout(AActor* LobbyTarget)
{
	if (GuestState != EGuestState::WaitingForCheckoutRequest)
	{
		return false;
	}

	return TryBeginCheckout(LobbyTarget);
}

bool AGuestCharacter::ActivateFromPool(const FVector& SpawnLocation, const FRotator& SpawnRotation,
	AActor* CounterTarget, const FDataTableRowHandle& InGuestTypeRow)
{
	// 대기 중인 손님만 꺼낼 수 있음
	if (GuestState != EGuestState::Inactive ||
		AssignedRoom != nullptr ||
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

	// 새 손님의 유형, 이동 속도, 퇴실 정책 적용
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

	StayId.Invalidate();
	CheckoutLobbyTarget = nullptr;
	CheckoutExitTarget = nullptr;
	bWillStayBehind = false;

	// 카운터 도착 판정에 사용하는 목적지도 갱신
	InitialMoveTarget = CounterTarget;
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
	if (AssignedRoom != nullptr)
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

	// 이전 투숙 정보 초기화
	StayId.Invalidate();
	InitialMoveTarget = nullptr;
	CheckoutLobbyTarget = nullptr;
	CheckoutExitTarget = nullptr;

	GuestTypeRow = FDataTableRowHandle{};
	CheckoutPolicy = EGuestCheckoutPolicy::Random;
	StayBehindChance = 0.0f;
	bWillStayBehind = false;

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
	&& IsValid(AssignedRoom.Get())
	&& StayId.IsValid()
	&& IsValid(CheckoutLobbyTarget.Get());
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

	RequestCheckout(CheckoutLobbyTarget.Get());
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
	if (InGuestTypeRow.DataTable == nullptr ||
		InGuestTypeRow.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Guest data table or row is not assigned."), *GetName());

		return false;
	}
	

	const FGuestTypeRow* GuestData =
		InGuestTypeRow.GetRow<FGuestTypeRow>(
		TEXT("AGuestCharacter::ApplyGuestData"));

	if (GuestData == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Failed to read guest row: %s"), *GetName(), *InGuestTypeRow.RowName.ToString());

		return false;
	}
	
	if (GuestData->BehaviorRow.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Guest behavior row is not assigned."));
		return false;
	}

	const FGuestBehaviorRow* BehaviorData =
		GuestData->BehaviorRow.GetRow<FGuestBehaviorRow>(
			TEXT("AGuestCharacter::ApplyGuestData"));

	if (BehaviorData == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read guest behavior row."));
		return false;
	}

	CheckoutPolicy = BehaviorData->CheckoutPolicy;
	StayBehindChance = FMath::Clamp(
		BehaviorData->StayBehindChance, 0.0f, 1.0f);
	
	GuestTypeRow = InGuestTypeRow;

	// 데이터 테이블의 값을 실제 이동 속도에 적용
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.0f, GuestData->WalkSpeed);

	return true;
}


