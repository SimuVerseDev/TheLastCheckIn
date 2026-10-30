#include "GuestPoolManager.h"

#include "Guests/GuestCharacter.h"
#include "Guests/GuestAIController.h"
#include "Guests/Party/GuestParty.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"

AGuestPoolManager::AGuestPoolManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(
		CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

AGuestCharacter* AGuestPoolManager::AcquireGuestInternal(AActor* SpawnPoint, AActor* CounterTarget,
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

AGuestCharacter* AGuestPoolManager::AcquireGuest(
	AActor* SpawnPoint,
	AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow)
{
	TArray<AActor*> SpawnPoints;
	SpawnPoints.Add(SpawnPoint);

	TArray<AGuestCharacter*> Members;

	// 한 명도 동일한 일행 생성 절차 사용
	if (!AcquireParty(
		SpawnPoints,
		CounterTarget,
		GuestTypeRow,
		1,
		Members))
	{
		return nullptr;
	}

	return Members[0];
}

bool AGuestPoolManager::AcquireParty(const TArray<AActor*>& SpawnPoints, AActor* CounterTarget,
                                     const FDataTableRowHandle& GuestTypeRow, int32 PartySize,
                                     TArray<AGuestCharacter*>& OutMembers)
{
	return TryAcquireParty(SpawnPoints, CounterTarget, GuestTypeRow, PartySize, OutMembers)
		== EGuestAcquireResult::Success;
}

bool AGuestPoolManager::ValidatePartyConfiguration(
	const TArray<AActor*>& SpawnPoints, AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow, int32 MaxPartySize) const
{
	if (MaxPartySize < 1 || 
		MaxPartySize > 4 || 
		SpawnPoints.Num() < MaxPartySize ||
		!IsValid(CounterTarget))
	{
		return false;
	}

	TSet<AActor*> UniquePoints;
	for (int32 Index = 0; Index < MaxPartySize; ++Index)
	{
		if (!IsValid(SpawnPoints[Index]) 
			|| UniquePoints.Contains(SpawnPoints[Index]))
		{
			return false;
		}
		UniquePoints.Add(SpawnPoints[Index]);
	}

	const AGuestCharacter* Defaults = GuestClass ? GuestClass.GetDefaultObject() : nullptr;
	
	const UBehaviorTree* Tree = IsValid(Defaults) ? Defaults->GetGuestBehaviorTree() : nullptr;
	
	if (!IsValid(Defaults) || 
		!Defaults->ShouldStartInPool() ||
		!IsValid(Tree) || 
		!IsValid(Tree->BlackboardAsset) || 
		InitialPoolSize < MaxPartySize)
	{
		return false;
	}

	const FGuestTypeRow* Type = GuestTypeRow.IsNull() ? nullptr :
		GuestTypeRow.GetRow<FGuestTypeRow>(TEXT("GuestPool.ValidatePartyConfiguration"));
	
	const FGuestBehaviorRow* Behavior = Type && !Type->BehaviorRow.IsNull() ?
		Type->BehaviorRow.GetRow<FGuestBehaviorRow>(TEXT("GuestPool.ValidateBehavior")) : nullptr;
	
	if (!Type || 
		!Behavior || 
		!FMath::IsFinite(Type->WalkSpeed) || 
		Type->WalkSpeed <= 0.0f)
	{
		return false;
	}
	return true;
}

EGuestAcquireResult AGuestPoolManager::TryAcquireParty(
	const TArray<AActor*>& SpawnPoints, AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow, int32 PartySize,
	TArray<AGuestCharacter*>& OutMembers)
{
	OutMembers.Reset();

	if (!ValidatePartyConfiguration(SpawnPoints, CounterTarget, GuestTypeRow, PartySize))
	{
		return EGuestAcquireResult::InvalidConfiguration;
	}

	// 사라진 NPC 참조 정리
	AvailableGuests.RemoveAll(
		[](const TObjectPtr<AGuestCharacter>& Guest)
		{
			return !IsValid(Guest.Get());
		});

	if (AvailableGuests.Num() < PartySize)
	{
		const int32 ValidActiveCount = ActiveGuests.FilterByPredicate(
			[](const TObjectPtr<AGuestCharacter>& Guest) { return IsValid(Guest.Get()); }).Num();
		if (HasActorBegunPlay() && 
			AvailableGuests.Num() + ValidActiveCount < PartySize)
		{

			return EGuestAcquireResult::InvalidConfiguration;
		}
		return EGuestAcquireResult::PoolExhausted;
	}

	// 도중에 실패하면 이미 꺼낸 손님들도 돌려보냄
	auto CancelCreatedGuests = [this, &OutMembers]()
	{
		for (AGuestCharacter* Member : OutMembers)
		{
			if (!IsValid(Member))
			{
				ActiveGuests.RemoveSingleSwap(Member);
				continue;
			}

			if (Member->CancelPoolActivation())
			{
				ActiveGuests.RemoveSingleSwap(Member);
				AvailableGuests.AddUnique(Member);
			}
			else
			{
				UE_LOG(LogTemp, Error,
				       TEXT("GuestPool: Failed to cancel arrival of %s."),
				       *GetNameSafe(Member));
			}
		}

		OutMembers.Reset();
	};

	// 기존 1명 등장 함수를 서로 다른 위치에서 호출
	for (int32 Index = 0; Index < PartySize; ++Index)
	{
		AGuestCharacter* Guest = AcquireGuestInternal(
			SpawnPoints[Index],
			CounterTarget,
			GuestTypeRow);

		if (!IsValid(Guest))
		{
			CancelCreatedGuests();
			return EGuestAcquireResult::SpawnFailed;
		}

		OutMembers.Add(Guest);
	}

	// 전원이 등장에 성공했으면 같은 일행으로 연결
	if (UGuestParty::Create(this, OutMembers) == nullptr)
	{
		CancelCreatedGuests();
		return EGuestAcquireResult::SpawnFailed;
	}

	return EGuestAcquireResult::Success;
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
