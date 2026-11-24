#include "Night/Anomaly/Fall/FallAnomaly.h"
#include "Night/Anomaly/Fall/FallObjectActor.h"
#include "TheLastCheckIn.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/GameFlow/HotelPhaseSettings.h"
#include "Core/Reputation/HotelReputationSubsystem.h"
#include "Shop/ShopPurchaseSubsystem.h"
#include "Shop/ShopPlacedProductComponent.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AFallAnomaly::AFallAnomaly()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AnomalyId = FName("Fall");
	DisplayName = NSLOCTEXT("Anomaly", "FallName", "낙하");

	/*
	22시에 시작해서 오브젝트들은 FallUntilHour 전까지 각자 따로 떨어짐
	상태 전이를 안 하므로 매니저의 악화는 어차피 동작하지 않음
	*/
	EarliestStartHour = 22;
	LatestStartHour = 22;
	EscalateAfterMinutes = 24 * 60;
}

void AFallAnomaly::BeginPlay()
{
	Super::BeginPlay();
	CollectFallObjects();

	UWorld* World = GetWorld();
	if (UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>())
	{
		Clock->OnTimeAdvanced.AddUniqueDynamic(this, &ThisClass::HandleTimeAdvanced);
	}
	if (UHotelPhaseSubsystem* Phase = World->GetSubsystem<UHotelPhaseSubsystem>())
	{
		Phase->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
		Phase->OnDayStarted.AddUniqueDynamic(this, &ThisClass::HandleDayStarted);
	}
	if (UShopPurchaseSubsystem* Shop = World->GetSubsystem<UShopPurchaseSubsystem>())
	{
		Shop->OnPurchaseConfirmed.AddUniqueDynamic(this, &ThisClass::HandlePurchaseConfirmed);
	}

	// 설치 미리보기는 구매 확정 전이라 이벤트가 없음 - 주기적으로 확인
	GetWorldTimerManager().SetTimer(SnapTimer, this, &ThisClass::CheckSnap, SnapCheckInterval, true);
}

void AFallAnomaly::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnTimeAdvanced.RemoveAll(this);
		}
		if (UHotelPhaseSubsystem* Phase = World->GetSubsystem<UHotelPhaseSubsystem>())
		{
			Phase->OnPhaseChanged.RemoveAll(this);
			Phase->OnDayStarted.RemoveAll(this);
		}
		if (UShopPurchaseSubsystem* Shop = World->GetSubsystem<UShopPurchaseSubsystem>())
		{
			Shop->OnPurchaseConfirmed.RemoveAll(this);
		}
	}
	GetWorldTimerManager().ClearTimer(SnapTimer);
	Super::EndPlay(EndPlayReason);
}

/*
Super::StartAnomaly를 호출하지 않음 - 상태는 Idle 유지, 연출 이벤트만 호출
항상 true 반환 (false면 매니저가 1시간 뒤 재시도)
*/
bool AFallAnomaly::StartAnomaly()
{
	const UGameClockSubsystem* Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>();
	if (!Clock)
	{
		return true;
	}

	PendingFalls.Reset();

	TArray<AFallObjectActor*> Candidates;
	for (AFallObjectActor* Object : FallObjects)
	{
		if (IsValid(Object) && Object->GetFallState() == EFallObjectState::Intact)
		{
			Candidates.Add(Object);
		}
	}
	if (Candidates.IsEmpty())
	{
		UE_LOG(LogAnomaly, Log, TEXT("[Fall] 떨어뜨릴 수 있는 오브젝트가 없음"));
		OnAnomalyStarted();
		return true;
	}

	/*
	지금부터 FallUntilHour(다음 날 새벽)까지 남은 시간
	*/
	const double Now = Clock->GetTotalGameSeconds();
	const double TimeOfDay = GameTime::GetSecondsOfDay(Now);
	double Until = GameTime::HoursToSeconds(FallUntilHour);
	if (Until <= TimeOfDay)
	{
		Until += GameTime::SecondsPerDay;
	}
	const double Window = Until - TimeOfDay;

	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		Candidates.Swap(i, FMath::RandRange(0, i));
	}
	const int32 MaxCount = FMath::Max(MinFallCount, MaxFallCount);
	const int32 Count = FMath::Min(FMath::RandRange(MinFallCount, MaxCount), Candidates.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		PendingFalls.Add(Candidates[i], Now + FMath::FRandRange(0.0, Window));
	}

	UE_LOG(LogAnomaly, Log, TEXT("[Fall] 오늘 밤 %d개 낙하 예정"), Count);
	OnAnomalyStarted();
	return true;
}

void AFallAnomaly::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	TArray<AFallObjectActor*> Due;
	for (auto It = PendingFalls.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key()))
		{
			It.RemoveCurrent();
			continue;
		}
		if (CurrentSeconds >= It.Value())
		{
			Due.Add(It.Key());
			It.RemoveCurrent();
		}
	}
	for (AFallObjectActor* Object : Due)
	{
		Object->Fall();
	}

	/*
	정산 시작 PenaltyLeadMinutes 전에 평판 판정
	정산 시작 순간에는 DailySettlementSubsystem이 먼저 그날 결과를 확정해서 감점이 정산 화면에 안 잡힘
	*/
	const double SettlementSeconds = GameTime::HoursToSeconds(UHotelPhaseSettings::Get()->SettlementStartHour);
	const double PenaltySeconds = GameTime::GetDayStartSeconds(GameTime::GetDayIndex(CurrentSeconds))
		+ SettlementSeconds - GameTime::MinutesToSeconds(PenaltyLeadMinutes);
	if (PreviousSeconds < PenaltySeconds && CurrentSeconds >= PenaltySeconds)
	{
		ApplyNightPenalty();
	}
}

void AFallAnomaly::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase == EHotelPhase::Settlement)
	{
		PendingFalls.Reset();
		// 시간 건너뛰기 등으로 정산 직전 판정을 놓친 경우 대비 - 이미 판정한 오브젝트는 표시가 지워져 두 번 깎이지 않음
		ApplyNightPenalty();
	}
}

/*
아침 - 제자리에서 깨진 것(거울)을 원래 상태로 되돌림, 나머지는 RestoreForNewDay가 걸러냄
*/
void AFallAnomaly::HandleDayStarted(int32 Day)
{
	for (AFallObjectActor* Object : FallObjects)
	{
		if (IsValid(Object))
		{
			Object->RestoreForNewDay();
		}
	}
}

/*
낙하 대상 상품을 사면 기억해 두고, 빈 자리 근처에 놓였는지 확인
설치 확정 위치가 멀면 일반 가구로 남고, 나중에 재배치·들고 오기로 근처에 오면 스냅
*/
void AFallAnomaly::HandlePurchaseConfirmed(FName ProductId, AActor* Product)
{
	if (ProductId.IsNone() || !IsValid(Product))
	{
		return;
	}

	const bool bFallProduct = FallObjects.ContainsByPredicate([ProductId](const AFallObjectActor* Object)
	{
		return IsValid(Object) && Object->GetRefillProductId() == ProductId;
	});
	if (!bFallProduct)
	{
		return;
	}

	PurchasedProducts.AddUnique(Product);
	TrySnapProduct(Product);
	UpdateSlotHints();
}

void AFallAnomaly::CheckSnap()
{
	PurchasedProducts.RemoveAll([](const TWeakObjectPtr<AActor>& Weak)
	{
		return !Weak.IsValid();
	});

	// TrySnapProduct가 목록에서 지우므로 복사본으로 돎
	const TArray<TWeakObjectPtr<AActor>> Products = PurchasedProducts;
	for (const TWeakObjectPtr<AActor>& Weak : Products)
	{
		TrySnapProduct(Weak.Get());
	}
	UpdateSlotHints();
}

bool AFallAnomaly::TrySnapProduct(AActor* Product)
{
	if (!IsValid(Product))
	{
		return false;
	}
	const UShopPlacedProductComponent* Placed = Product->FindComponentByClass<UShopPlacedProductComponent>();
	if (!Placed || Placed->IsRelocating())
	{
		return false;
	}
	// 들고 있는 동안은 기다림 - 놓은 뒤에 판정
	if (const AHKPhysicsPickupActor* Pickup = Cast<AHKPhysicsPickupActor>(Product))
	{
		if (Pickup->IsCarried())
		{
			return false;
		}
	}

	const FName ProductId = Placed->GetProductId();
	AFallObjectActor* Nearest = nullptr;
	double NearestDist = SnapRadius;
	for (AFallObjectActor* Object : FallObjects)
	{
		if (!IsValid(Object) || Object->GetFallState() != EFallObjectState::Empty || Object->GetRefillProductId() != ProductId)
		{
			continue;
		}
		const double Dist = FVector::Dist2D(Object->GetActorLocation(), Product->GetActorLocation());
		if (Dist <= NearestDist)
		{
			NearestDist = Dist;
			Nearest = Object;
		}
	}
	if (!Nearest || !Nearest->Restore())
	{
		return false;
	}

	if (UHotelReputationSubsystem* Reputation = GetWorld()->GetSubsystem<UHotelReputationSubsystem>())
	{
		Reputation->AddReputation(EReputationReason::CleanRoomAssigned);
	}

	// 원래 오브젝트가 복구됐으니 산 상품은 제거 - 구매·재배치 처리가 끝난 뒤
	PurchasedProducts.Remove(Product);
	Product->SetActorHiddenInGame(true);
	Product->SetActorEnableCollision(false);
	TWeakObjectPtr<AActor> WeakProduct = Product;
	GetWorldTimerManager().SetTimerForNextTick([WeakProduct]()
	{
		if (AActor* ToDestroy = WeakProduct.Get())
		{
			ToDestroy->Destroy();
		}
	});
	UE_LOG(LogAnomaly, Log, TEXT("[Fall] '%s' 자리로 스냅 - 복구"), *Nearest->GetName());
	return true;
}

/*
놓으려는 상품 = 상점 설치 미리보기 중인 상품 + 재배치 중이거나 들고 있는 산 상품
*/
void AFallAnomaly::UpdateSlotHints()
{
	TSet<FName> ActiveIds;
	if (const UShopPurchaseSubsystem* Shop = GetWorld()->GetSubsystem<UShopPurchaseSubsystem>())
	{
		if (Shop->HasPendingPurchase())
		{
			ActiveIds.Add(Shop->GetPendingProductId());
		}
	}
	for (const TWeakObjectPtr<AActor>& Weak : PurchasedProducts)
	{
		const AActor* Product = Weak.Get();
		const UShopPlacedProductComponent* Placed = Product ? Product->FindComponentByClass<UShopPlacedProductComponent>() : nullptr;
		if (!Placed)
		{
			continue;
		}
		const AHKPhysicsPickupActor* Pickup = Cast<AHKPhysicsPickupActor>(Product);
		if (Placed->IsRelocating() || (Pickup && Pickup->IsCarried()))
		{
			ActiveIds.Add(Placed->GetProductId());
		}
	}

	for (AFallObjectActor* Object : FallObjects)
	{
		if (IsValid(Object))
		{
			Object->SetSlotHintVisible(ActiveIds.Contains(Object->GetRefillProductId()));
		}
	}
}

void AFallAnomaly::CollectFallObjects()
{
	FallObjects.Reset();
	for (TActorIterator<AFallObjectActor> It(GetWorld()); It; ++It)
	{
		FallObjects.Add(*It);
	}
}

/*
오늘 밤 떨어졌는데 봉투에 담지 않은 것(잔해 그대로)마다 평판 감소
판정 후 표시를 지워서 다음 날부터는 방치해도 감소 없음
*/
void AFallAnomaly::ApplyNightPenalty()
{
	UHotelReputationSubsystem* Reputation = GetWorld()->GetSubsystem<UHotelReputationSubsystem>();
	for (AFallObjectActor* Object : FallObjects)
	{
		if (!IsValid(Object) || !Object->HasFallenTonight())
		{
			continue;
		}
		// 봉투에 담으면 HasFallenTonight가 false가 되므로 여기 남은 건 전부 미처리
		if (Reputation)
		{
			Reputation->AddReputation(EReputationReason::Complaint);
			UE_LOG(LogAnomaly, Log, TEXT("[Fall] '%s' 미처리 - 평판 감소"), *Object->GetName());
		}
		Object->ClearFallenTonight();
	}
}
