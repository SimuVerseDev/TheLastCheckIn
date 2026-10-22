#include "GuestPoolManager.h"

#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

AGuestPoolManager::AGuestPoolManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(
		CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

AGuestCharacter* AGuestPoolManager::AcquireGuest(AActor* SpawnPoint, AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow)
{
	if (!IsValid(SpawnPoint) || !IsValid(CounterTarget))
	{
		return nullptr;
	}

	// 유효하지 않은 대기 객체 정리
	while (!AvailableGuests.IsEmpty() &&
		!IsValid(AvailableGuests.Last().Get()))
	{
		AvailableGuests.RemoveAt(AvailableGuests.Num() - 1);
	}

	// 모두 사용 중이면 입장 보류
	if (AvailableGuests.IsEmpty())
	{
		return nullptr;
	}

	AGuestCharacter* Guest = AvailableGuests.Last().Get();

	const FVector SpawnLocation = SpawnPoint->GetActorLocation();
	const FRotator SpawnRotation = SpawnPoint->GetActorRotation();

	const UCapsuleComponent* Capsule = Guest->GetCapsuleComponent();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Guest);
	QueryParams.AddIgnoredActor(this);

	const bool bSpawnBlocked =
		GetWorld()->OverlapBlockingTestByProfile(
			SpawnLocation,
			SpawnRotation.Quaternion(),
			Capsule->GetCollisionProfileName(),
			FCollisionShape::MakeCapsule(
				Capsule->GetScaledCapsuleRadius(),
				Capsule->GetScaledCapsuleHalfHeight()),
			QueryParams);

	if (bSpawnBlocked)
	{
		return nullptr;
	}

	AvailableGuests.RemoveAt(AvailableGuests.Num() - 1);

	if (!Guest->ActivateFromPool(
		SpawnLocation,
		SpawnRotation,
		CounterTarget,
		GuestTypeRow))
	{
		// 입장 실패 시 대기 목록으로 복구
		AvailableGuests.Add(Guest);
		return nullptr;
	}

	ActiveGuests.Add(Guest);

	return Guest;
}

bool AGuestPoolManager::ReleaseGuest(AGuestCharacter* Guest)
{
	if (!IsValid(Guest))
	{
		return false;
	}

	// 다른 풀의 손님이나 이미 반환된 손님 차단
	if (!ActiveGuests.Contains(Guest))
	{
		return false;
	}

	// 출구 도착이 완료된 손님만 반환
	if (Guest->GetGuestState() != EGuestState::ExitCompleted)
	{
		return false;
	}

	if (!Guest->DeactivateToPool())
	{
		return false;
	}

	ActiveGuests.RemoveSingleSwap(Guest);
	AvailableGuests.Add(Guest);

	return true;
}

// Called when the game starts or when spawned
void AGuestPoolManager::BeginPlay()
{
	Super::BeginPlay();

	if (!GuestClass)
	{
		return;
	}

	const AGuestCharacter* GuestDefaults =
		GuestClass.GetDefaultObject();

	if (!IsValid(GuestDefaults) ||
		!GuestDefaults->ShouldStartInPool())
	{
		return;
	}

	const int32 PoolSize = FMath::Max(1, InitialPoolSize);

	AvailableGuests.Reserve(PoolSize);
	ActiveGuests.Reserve(PoolSize);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;

	// 생성 후 숨김, 충돌 해제되므로 같은 위치에 미리 생성 가능
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		AGuestCharacter* Guest =
			GetWorld()->SpawnActor<AGuestCharacter>(
				GuestClass,
				GetActorLocation(),
				GetActorRotation(),
				SpawnParams);

		if (!IsValid(Guest))
		{
			continue;
		}
		
		if (!IsValid(Guest->GetController()))
		{
			Guest->SpawnDefaultController();
		}

		AGuestAIController* GuestController =
			Cast<AGuestAIController>(Guest->GetController());

		if (!IsValid(GuestController) ||
			!Guest->DeactivateToPool())
		{
			// 초기화에 실패한 객체만 정리
			if (AController* Controller = Guest->GetController())
			{
				Controller->Destroy();
			}

			Guest->Destroy();
			continue;
		}
		
		Guest->SetPoolManager(this);
		AvailableGuests.Add(Guest);
	}

	if (AvailableGuests.Num() != PoolSize)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GuestPool: Created %d / %d guests."),
			AvailableGuests.Num(), PoolSize);
	}
}




