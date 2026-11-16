#include "Shop/ShopPurchaseSubsystem.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Shop/ShopPlacedProductComponent.h"

void UShopPurchaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Economy = Collection.InitializeDependency<UHotelEconomySubsystem>();
}

void UShopPurchaseSubsystem::Deinitialize()
{
	CancelPurchase();
	Super::Deinitialize();
}

void UShopPurchaseSubsystem::ClearPending()
{
	bPending = false;
	PendingId = NAME_None;
	PendingClass = nullptr;
	PendingMesh = nullptr;
	PendingProduct = FShopProductRow();
}

bool UShopPurchaseSubsystem::BeginPurchase(UDataTable* Table, FName ProductId, FText& Error)
{
	Error = FText::GetEmpty();
	if (bChanging || bPending)
	{
		Error = NSLOCTEXT("Shop", "AlreadyPending", "먼저 현재 상품의 배치를 확정하거나 취소하세요.");
		return false;
	}
	TGuardValue<bool> Guard(bChanging, true);
	const auto* Row = Table && Table->GetRowStruct() == FShopProductRow::StaticStruct()
		? Table->FindRow<FShopProductRow>(ProductId, TEXT("Purchase"), false) : nullptr;
	if (!Row || !Row->bAvailable || Row->Price <= 0 || !FMath::IsFinite(Row->Scale) || Row->Scale <= 0)
	{
		Error = NSLOCTEXT("Shop", "InvalidProduct", "구매할 수 없는 상품입니다. 상품 데이터를 확인하세요.");
		return false;
	}
	UStaticMesh* Mesh = Row->PreviewMesh.LoadSynchronous();
	UClass* Class = Row->ActorClass.IsNull() ? AStaticMeshActor::StaticClass() : Row->ActorClass.LoadSynchronous();
	if (!Mesh || !Class || !Class->IsChildOf(AActor::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		Error = NSLOCTEXT("Shop", "MissingAsset", "상품의 메시 또는 액터 클래스를 불러올 수 없습니다.");
		return false;
	}
	// Copy the charged price and assets: later edits to the catalog cannot change the refund.
	PendingProduct = *Row;
	PendingId = ProductId;
	PendingMesh = Mesh;
	PendingClass = Class;
	if (!Economy || !Economy->TrySpend(PendingProduct.Price))
	{
		ClearPending();
		Error = NSLOCTEXT("Shop", "InsufficientFunds", "소지금이 부족합니다.");
		return false;
	}
	bPending = true;
	return true;
}

bool UShopPurchaseSubsystem::CancelPurchase()
{
	if (bChanging || !bPending) return false;
	TGuardValue<bool> Guard(bChanging, true);
	const int32 Refund = PendingProduct.Price;
	ClearPending(); // Clear before broadcasting the balance change, so callbacks cannot refund twice.
	if (Economy) Economy->RefundPurchase(Refund);
	return true;
}

AActor* UShopPurchaseSubsystem::ConfirmPurchase(const FTransform& Transform, FText& Error)
{
	Error = FText::GetEmpty();
	if (bChanging || !bPending) return nullptr;
	AActor* Product = nullptr;
	FName ProductId = PendingId;
	{
		TGuardValue<bool> Guard(bChanging, true);
		if (!Transform.ContainsNaN() && Transform.IsValid())
		{
			FTransform Placement = Transform;
			Placement.SetScale3D(FVector(PendingProduct.Scale));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			// Keep runtime purchases in the persistent level, independent of the read-only gameplay sublevel.
			Params.OverrideLevel = GetWorld()->PersistentLevel;
			Params.bDeferConstruction = true;
			Product = GetWorld()->SpawnActor<AActor>(PendingClass, Placement, Params);
			if (Product)
			{
				if (PendingProduct.ActorClass.IsNull())
				{
					auto* MeshActor = CastChecked<AStaticMeshActor>(Product);
					MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
					MeshActor->GetStaticMeshComponent()->SetStaticMesh(PendingMesh);
					MeshActor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
				}
				Product->FinishSpawning(Placement);
				if (!IsValid(Product) || Product->IsActorBeingDestroyed()) Product = nullptr;
			}
		}
		if (Product)
		{
			auto* Placed = NewObject<UShopPlacedProductComponent>(Product);
			Placed->InitializeProduct(PendingId, PendingMesh);
			Product->AddInstanceComponent(Placed);
			Placed->RegisterComponent();
			ClearPending();
		}
	}
	if (!Product)
	{
		CancelPurchase();
		Error = NSLOCTEXT("Shop", "SpawnFailed", "물건을 생성할 수 없어 구매 금액을 환불했습니다.");
		return nullptr;
	}
	OnPurchaseConfirmed.Broadcast(ProductId, Product);
	return Product;
}
