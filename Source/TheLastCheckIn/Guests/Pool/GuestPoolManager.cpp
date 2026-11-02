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
#include "CheckIn/CheckInCounter.h"

AGuestPoolManager::AGuestPoolManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(
		CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

// 풀 초기화: 손님과 AI 컨트롤러를 미리 생성하고 비활성 상태로 보관한다.
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

	// 생성 후 숨김과 충돌 해제를 적용하므로 같은 위치에 미리 생성할 수 있다.
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
			// 사용할 수 없는 손님은 컨트롤러와 함께 제거한다.
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

// 입장 설정 검사: 등장 위치, 카운터 자리, 손님 클래스와 데이터를 확인한다.
bool AGuestPoolManager::ValidatePartyConfiguration(
	const TArray<AActor*>& SpawnPoints,
	AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow,
	int32 MaxPartySize) const
{
	if (MaxPartySize < 1 ||
		MaxPartySize > 4 ||
		SpawnPoints.Num() < MaxPartySize ||
		!IsValid(CounterTarget))
	{
		return false;
	}

	const ACheckInCounter* Counter = Cast<ACheckInCounter>(CounterTarget);

	if (!IsValid(Counter) || !Counter->HasSlots(MaxPartySize))
	{
		return false;
	}

	// 일행 구성원마다 서로 다른 등장 위치가 필요하다.
	TSet<AActor*> UniquePoints;

	for (int32 Index = 0; Index < MaxPartySize; ++Index)
	{
		if (!IsValid(SpawnPoints[Index]) ||
			UniquePoints.Contains(SpawnPoints[Index]))
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

	const FGuestTypeRow* Type = GuestTypeRow.IsNull()
		? nullptr
		: GuestTypeRow.GetRow<FGuestTypeRow>(TEXT("GuestPool.ValidatePartyConfiguration"));

	const FGuestBehaviorRow* Behavior = Type && !Type->BehaviorRow.IsNull()
		? Type->BehaviorRow.GetRow<FGuestBehaviorRow>(
			TEXT("GuestPool.ValidateBehavior"))
		: nullptr;

	if (!Type ||
		!Behavior ||
		!FMath::IsFinite(Type->WalkSpeed) ||
		Type->WalkSpeed <= 0.0f)
	{
		return false;
	}

	return true;
}

// 일행 입장: 손님 활성화, 일행 생성, 카운터 등록을 마친 뒤 AI를 시작한다.
EGuestAcquireResult AGuestPoolManager::TryAcquireParty(
	const TArray<AActor*>& SpawnPoints,
	AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow,
	int32 PartySize,
	TArray<AGuestCharacter*>& OutMembers)
{
	OutMembers.Reset();

	if (!ValidatePartyConfiguration(SpawnPoints, CounterTarget, GuestTypeRow, PartySize))
	{
		return EGuestAcquireResult::InvalidConfiguration;
	}

	ACheckInCounter* Counter = Cast<ACheckInCounter>(CounterTarget);

	if (!IsValid(Counter))
	{
		return EGuestAcquireResult::InvalidConfiguration;
	}

	if (!Counter->CanAcceptParty())
	{
		return EGuestAcquireResult::CounterBusy;
	}

	// 풀 수량을 판단하기 전에 사라진 대기 손님의 참조를 제거한다.
	AvailableGuests.RemoveAll(
		[](const TObjectPtr<AGuestCharacter>& Guest)
		{
			return !IsValid(Guest.Get());
		});

	if (AvailableGuests.Num() < PartySize)
	{
		// 초기화 후 전체 유효 인원도 부족하면 설정 오류, 반환을 기다릴 수 있으면 풀 부족이다.
		const int32 ValidActiveCount = ActiveGuests.FilterByPredicate(
			[](const TObjectPtr<AGuestCharacter>& Guest)
			{
				return IsValid(Guest.Get());
			}).Num();

		if (HasActorBegunPlay() &&
			AvailableGuests.Num() + ValidActiveCount < PartySize)
		{
			return EGuestAcquireResult::InvalidConfiguration;
		}

		return EGuestAcquireResult::PoolExhausted;
	}

	// 일행 구성이나 AI 시작에 실패하면 이번에 활성화한 손님들을 돌려놓는다.
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

	// 구성원마다 지정된 위치에서 활성화한다. 한 명이라도 실패하면 전원 취소한다.
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

	UGuestParty* NewParty = UGuestParty::Create(this, OutMembers);

	if (!IsValid(NewParty))
	{
		CancelCreatedGuests();
		return EGuestAcquireResult::SpawnFailed;
	}

	if (!Counter->RegisterParty(NewParty))
	{
		CancelCreatedGuests();
		return EGuestAcquireResult::CounterBusy;
	}

	// 카운터가 각 구성원의 자리를 찾을 수 있도록 등록을 마친 뒤 AI를 시작한다.
	for (AGuestCharacter* Member : OutMembers)
	{
		AGuestAIController* GuestController =
			Cast<AGuestAIController>(Member->GetController());

		if (!IsValid(GuestController) ||
			!GuestController->StartGuestAI(Counter))
		{
			Counter->UnregisterParty(NewParty);
			CancelCreatedGuests();
			return EGuestAcquireResult::SpawnFailed;
		}
	}

	return EGuestAcquireResult::Success;
}

// 개별 손님 활성화: 등장 위치가 비어 있으면 대기 목록에서 꺼낸다.
AGuestCharacter* AGuestPoolManager::AcquireGuestInternal(
	AActor* SpawnPoint,
	AActor* CounterTarget,
	const FDataTableRowHandle& GuestTypeRow)
{
	if (!IsValid(SpawnPoint) || !IsValid(CounterTarget))
	{
		return nullptr;
	}

	// 꺼낼 위치에 남아 있는 무효 참조를 제거한다.
	while (!AvailableGuests.IsEmpty() &&
		!IsValid(AvailableGuests.Last().Get()))
	{
		AvailableGuests.RemoveAt(AvailableGuests.Num() - 1);
	}

	// 대기 중인 손님이 없으면 입장을 보류한다.
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
		// 활성화에 실패하면 대기 목록으로 복구한다.
		AvailableGuests.Add(Guest);
		return nullptr;
	}

	ActiveGuests.Add(Guest);

	return Guest;
}

// 손님 반환: 퇴실을 완료한 손님을 비활성화하고 대기 목록으로 옮긴다.
bool AGuestPoolManager::ReleaseGuest(AGuestCharacter* Guest)
{
	if (!IsValid(Guest))
	{
		return false;
	}

	// 이 풀에서 사용 중인 손님만 반환할 수 있다.
	if (!ActiveGuests.Contains(Guest))
	{
		return false;
	}

	// 출구 도착이 완료된 손님만 정상 반환한다.
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
