#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Shop/ShopPurchaseSubsystem.h"
#include "Shop/ShopPlacedProductComponent.h"
#include "Shop/ShopPlacementComponent.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

namespace
{
	UWorld* MakeShopTestWorld()
	{
		const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		return World;
	}

	void DestroyShopTestWorld(UWorld* World)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	AStaticMeshActor* BuyTestProduct(UWorld* World, const FVector& Location)
	{
		World->GetSubsystem<UHotelEconomySubsystem>()->AddIncome(100);
		auto* Table = NewObject<UDataTable>(World);
		Table->RowStruct = FShopProductRow::StaticStruct();
		FShopProductRow Row;
		Row.Price = 25;
		Row.PreviewMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Table->AddRow(TEXT("RelocationTest"), Row);
		FText Error;
		auto* Shop = World->GetSubsystem<UShopPurchaseSubsystem>();
		if (!Shop->BeginPurchase(Table, TEXT("RelocationTest"), Error)) return nullptr;
		return Cast<AStaticMeshActor>(Shop->ConfirmPurchase(FTransform(Location), Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopRelocationStateTest, "TheLastCheckIn.Shop.RelocationState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopRelocationStateTest::RunTest(const FString& Parameters)
{
	auto* World = MakeShopTestWorld();
	auto* Product = BuyTestProduct(World, FVector(250, 0, 50));
	if (!TestNotNull(TEXT("Purchased actor exists"), Product)) { DestroyShopTestWorld(World); return false; }
	auto* Marker = Product->FindComponentByClass<UShopPlacedProductComponent>();
	if (!TestNotNull(TEXT("Purchases automatically receive relocation identity"), Marker)) { DestroyShopTestWorld(World); return false; }
	auto* Economy = World->GetSubsystem<UHotelEconomySubsystem>();
	TestEqual(TEXT("Stable product id retained"), Marker->GetProductId(), FName(TEXT("RelocationTest")));
	Product->Tags.Add(TEXT("CustomRuntimeState"));
	auto* Light = NewObject<UPointLightComponent>(Product);
	Product->AddInstanceComponent(Light);
	Light->SetupAttachment(Product->GetRootComponent());
	Light->SetRelativeLocation(FVector(12, 5, 70));
	Light->SetIntensity(4321);
	Light->RegisterComponent();
	const FTransform Original = Product->GetActorTransform();
	const FTransform LightOriginal = Light->GetComponentTransform();
	TestTrue(TEXT("Can relocate after purchase"), Marker->CanRelocate());
	TestTrue(TEXT("Begin relocation"), Marker->BeginRelocation());
	TestTrue(TEXT("Original temporarily hidden"), Product->IsHidden());
	TestFalse(TEXT("Original collision excluded during preview"), Product->GetActorEnableCollision());
	TestFalse(TEXT("Cannot reserve same actor twice"), Marker->BeginRelocation());
	Marker->CancelRelocation();
	Marker->CancelRelocation();
	TestTrue(TEXT("Cancellation restores exact transform"), Product->GetActorTransform().Equals(Original));
	TestTrue(TEXT("Cancellation preserves child transform"), Light->GetComponentTransform().Equals(LightOriginal));
	TestFalse(TEXT("Cancellation unhides original"), Product->IsHidden());
	TestTrue(TEXT("Cancellation restores collision"), Product->GetActorEnableCollision());
	TestEqual(TEXT("No refund for a purchased product"), Economy->GetCapital(), 75.0f);
	TestTrue(TEXT("Can begin again"), Marker->BeginRelocation());
	const FTransform Destination(FRotator(0, 90, 0), FVector(600, 120, 50));
	TestTrue(TEXT("Move existing actor"), Marker->FinishRelocation(Destination));
	TestTrue(TEXT("Actor reached destination"), Product->GetActorTransform().Equals(Destination));
	TestTrue(TEXT("Child light moves with original actor"), Light->GetComponentTransform().Equals(LightOriginal.GetRelativeTransform(Original) * Destination));
	TestEqual(TEXT("Light state preserved"), Light->Intensity, 4321.0f);
	TestTrue(TEXT("Runtime state preserved"), Product->Tags.Contains(TEXT("CustomRuntimeState")));
	Marker->CancelRelocation();
	TestTrue(TEXT("Cancel after commit does not undo it"), Product->GetActorTransform().Equals(Destination));
	TestEqual(TEXT("Moving does not charge again"), Economy->GetCapital(), 75.0f);
	TestEqual(TEXT("Moving does not change income"), Economy->GetTodayIncome(), 100.0f);
	// A physics-enabled purchased actor must retain physics on both rollback and commit.
	auto* Mesh = Product->GetStaticMeshComponent();
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(FVector(20, 0, 0));
	TestTrue(TEXT("Relocate a physics product"), Marker->BeginRelocation());
	TestFalse(TEXT("Physics paused for preview"), Mesh->IsSimulatingPhysics());
	Marker->CancelRelocation();
	TestTrue(TEXT("Physics restored on cancel"), Mesh->IsSimulatingPhysics());
	TestTrue(TEXT("Velocity restored on cancel"), Mesh->GetPhysicsLinearVelocity().Equals(FVector(20, 0, 0)));
	TestTrue(TEXT("Relocate physics product again"), Marker->BeginRelocation());
	TestTrue(TEXT("Physics product commits"), Marker->FinishRelocation(Original));
	TestTrue(TEXT("Physics restored on commit"), Mesh->IsSimulatingPhysics());
	TestTrue(TEXT("Commit does not throw the product"), Mesh->GetPhysicsLinearVelocity().IsNearlyZero());
	Product->SetOwner(World->SpawnActor<AActor>());
	TestFalse(TEXT("Equipped/owned actors cannot be relocated"), Marker->CanRelocate());
	Product->SetOwner(nullptr);
	Product->DispatchBeginPlay();
	TestTrue(TEXT("EndPlay rollback setup"), Marker->BeginRelocation());
	Marker->DestroyComponent();
	TestFalse(TEXT("Removing relocation component unhides actor"), Product->IsHidden());
	TestTrue(TEXT("Removing relocation component restores collision"), Product->GetActorEnableCollision());
	DestroyShopTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopRelocationInputTest, "TheLastCheckIn.Shop.RelocationRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopRelocationInputTest::RunTest(const FString& Parameters)
{
	auto* World = MakeShopTestWorld();
	auto* Product = BuyTestProduct(World, FVector(250, 0, 50));
	if (!TestNotNull(TEXT("Product created"), Product)) { DestroyShopTestWorld(World); return false; }
	auto* Marker = Product->FindComponentByClass<UShopPlacedProductComponent>();
	auto* PC = World->SpawnActor<APlayerController>();
	auto* Pawn = World->SpawnActor<APawn>();
	auto* PawnRoot = NewObject<USceneComponent>(Pawn);
	Pawn->SetRootComponent(PawnRoot);
	PawnRoot->RegisterComponent();
	Pawn->SetActorLocation(FVector(0, 0, 100));
	PC->Possess(Pawn);
	if (!PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		PC->PlayerCameraManager->InitializeFor(PC);
	}
	auto* Camera = World->SpawnActor<ACameraActor>(FVector(0, 0, 160), FRotator::ZeroRotator);
	PC->SetViewTarget(Camera);
	PC->PlayerCameraManager->UpdateCamera(0);
	auto* Placement = NewObject<UShopPlacementComponent>(PC);
	PC->AddInstanceComponent(Placement);
	Placement->RegisterComponent();
	Placement->Purchase = World->GetSubsystem<UShopPurchaseSubsystem>();
	const FTransform Original = Product->GetActorTransform();
	// A release before the hold threshold must not move or charge anything.
	Placement->bHoldingProduct = true;
	Placement->HoldCandidate = Marker;
	Placement->HoldElapsed = 0.1f;
	TestTrue(TEXT("Short click is consumed"), Placement->HandlePlacementRelease());
	TestFalse(TEXT("Short click clears hold"), Placement->HasPlacementInteraction());
	TestTrue(TEXT("Short click leaves original unchanged"), Product->GetActorTransform().Equals(Original));
	const auto StartMove = [&]()
	{
		Marker->BeginRelocation();
		Placement->RelocatingProduct = Marker;
		Placement->PlacementPawn = Pawn;
		Placement->CreatePreview(Marker->GetPreviewMesh(), Product->GetActorScale3D());
		Placement->Yaw = 0;
	};
	StartMove();
	TestTrue(TEXT("Release without a surface is consumed"), Placement->HandlePlacementRelease());
	TestTrue(TEXT("Invalid release restores original location"), Product->GetActorTransform().Equals(Original));
	TestFalse(TEXT("Invalid release unhides original"), Product->IsHidden());
	TestFalse(TEXT("Invalid release removes preview"), Placement->IsPlacing());
	auto* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0, 0, -50), FRotator::ZeroRotator);
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(Marker->GetPreviewMesh());
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetActorScale3D(FVector(20, 20, 1));
	StartMove();
	Placement->UpdatePreview();
	TestTrue(TEXT("Empty nearby floor accepts placement"), Placement->bValidPlacement);
	const FTransform Expected = Placement->Preview->GetActorTransform();
	TestTrue(TEXT("Valid release consumed"), Placement->HandlePlacementRelease());
	TestTrue(TEXT("Release moves the original actor to preview"), Product->GetActorTransform().Equals(Expected));
	TestFalse(TEXT("Release ends placement"), Placement->IsPlacing());
	TestTrue(TEXT("Actor remains reusable"), Marker->CanRelocate());
	StartMove();
	Placement->CancelPlacement();
	TestTrue(TEXT("Escape preserves most recently committed location"), Product->GetActorTransform().Equals(Expected));
	TestEqual(TEXT("Release/cancel never changes money"), World->GetSubsystem<UHotelEconomySubsystem>()->GetCapital(), 75.0f);
	DestroyShopTestWorld(World);
	return true;
}
#endif
