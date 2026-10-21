#include "GuestCharacter.h"

#include "GuestAIController.h"
#include "Data/GuestDataTypes.h"
#include "GameFramework/CharacterMovementComponent.h"


AGuestCharacter::AGuestCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AIControllerClass = AGuestAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[%s] Guest data table or row is not assigned."),
			*GetName()
		);

		return false;
	}
	

	const FGuestTypeRow* GuestData =
		InGuestTypeRow.GetRow<FGuestTypeRow>(
		TEXT("AGuestCharacter::ApplyGuestData"));

	if (GuestData == nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[%s] Failed to read guest row: %s"),
			*GetName(),
			*InGuestTypeRow.RowName.ToString()
		);

		return false;
	}
	
	GuestTypeRow = InGuestTypeRow;

	// 데이터 테이블의 값을 실제 이동 속도에 적용
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.0f, GuestData->WalkSpeed);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[%s] Applied guest row: %s, WalkSpeed: %.1f"),
		*GetName(),
		*GuestTypeRow.RowName.ToString(),
		GetCharacterMovement()->MaxWalkSpeed
	);

	return true;
}


