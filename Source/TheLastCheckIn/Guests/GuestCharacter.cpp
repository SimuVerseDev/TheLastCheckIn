#include "GuestCharacter.h"

#include "GuestAIController.h"
#include "Data/GuestDataTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rooms/RoomActor.h"


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
			
			return true;
		}

	default:
		return false;
	}
}

bool AGuestCharacter::TryBeginCheckout(AActor* LobbyTarget)
{
	if (GuestState!=EGuestState::Staying)
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
	
	CheckoutLobbyTarget = LobbyTarget;
	GuestState = EGuestState::MovingToLobby;
	
	//로비를 새 이동 목적지로 등록
	if (!GuestController->SetGuestMoveTarget(LobbyTarget))
	{
		CheckoutLobbyTarget = nullptr;
		GuestState = EGuestState::Staying;
		
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

void AGuestCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	ApplyGuestData(GuestTypeRow);
	
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
	
	GuestTypeRow = InGuestTypeRow;

	// 데이터 테이블의 값을 실제 이동 속도에 적용
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.0f, GuestData->WalkSpeed);

	return true;
}


