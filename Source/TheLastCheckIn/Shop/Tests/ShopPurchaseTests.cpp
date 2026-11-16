#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Shop/ShopPurchaseSubsystem.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#if WITH_EDITOR
#include "Desktop/DesktopSurfaceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Editor.h"
#include "Components/TextBlock.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopPurchaseTest, "TheLastCheckIn.Shop.PurchaseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopPurchaseTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Shop = World->GetSubsystem<UShopPurchaseSubsystem>();
	auto* Economy = World->GetSubsystem<UHotelEconomySubsystem>();
	Economy->AddIncome(100);
	auto* Table = NewObject<UDataTable>(World);
	Table->RowStruct = FShopProductRow::StaticStruct();
	FShopProductRow Row;
	Row.Price = 40;
	Row.PreviewMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Table->AddRow(TEXT("Chair"), Row);
	FText Error;
	TestFalse(TEXT("Unknown row rejected"), Shop->BeginPurchase(Table, TEXT("Missing"), Error));
	TestFalse(TEXT("Null catalog rejected"), Shop->BeginPurchase(nullptr, TEXT("Chair"), Error));
	TestEqual(TEXT("Invalid requests never debit"), Economy->GetCapital(), 100.0f);
	TestTrue(TEXT("Purchase reserves funds"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	TestEqual(TEXT("Funds deducted at purchase"), Economy->GetCapital(), 60.0f);
	TestTrue(TEXT("Awaiting placement"), Shop->HasPendingPurchase());
	TestFalse(TEXT("Second purchase blocked while pending"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Row.Price = 80;
	Table->AddRow(TEXT("Chair"), Row);
	TestEqual(TEXT("Price snapshot is immutable"), Shop->GetReservedPrice(), 40);
	TestTrue(TEXT("Cancel refunds"), Shop->CancelPurchase());
	TestEqual(TEXT("Refund uses charged price"), Economy->GetCapital(), 100.0f);
	TestEqual(TEXT("Refund is not income"), Economy->GetTodayIncome(), 100.0f);
	TestFalse(TEXT("Repeated cancel ignored"), Shop->CancelPurchase());
	TestNull(TEXT("Confirm after cancellation ignored"), Shop->ConfirmPurchase(FTransform::Identity, Error));
	TestEqual(TEXT("Cannot duplicate refund"), Economy->GetCapital(), 100.0f);
	TestTrue(TEXT("Can purchase again"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	AActor* Product = Shop->ConfirmPurchase(FTransform(FVector(0, 0, 100)), Error);
	TestNotNull(TEXT("Confirmation creates product"), Product);
	if (auto* MeshActor = Cast<AStaticMeshActor>(Product))
		TestTrue(TEXT("Default product uses catalog mesh"), MeshActor->GetStaticMeshComponent()->GetStaticMesh() == Row.PreviewMesh.Get());
	TestFalse(TEXT("Confirmation consumes reservation"), Shop->HasPendingPurchase());
	TestEqual(TEXT("Confirmation does not charge twice"), Economy->GetCapital(), 20.0f);
	TestFalse(TEXT("Cannot refund confirmed product"), Shop->CancelPurchase());
	TestNull(TEXT("Cannot confirm twice"), Shop->ConfirmPurchase(FTransform::Identity, Error));
	TestFalse(TEXT("Insufficient funds rejected"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	TestEqual(TEXT("Rejected purchase leaves balance intact"), Economy->GetCapital(), 20.0f);
	Row.Price = 10;
	Row.bAvailable = false;
	Table->AddRow(TEXT("Chair"), Row);
	TestFalse(TEXT("Unavailable row rejected"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Row.bAvailable = true;
	Row.Price = -1;
	Table->AddRow(TEXT("Chair"), Row);
	TestFalse(TEXT("Negative price rejected"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Row.Price = 0;
	Table->AddRow(TEXT("Chair"), Row);
	TestFalse(TEXT("Zero price rejected"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Row.Price = 10;
	Row.PreviewMesh.Reset();
	Table->AddRow(TEXT("Chair"), Row);
	TestFalse(TEXT("Missing preview asset rejected before debit"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Row.PreviewMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Row.ActorClass = AActor::StaticClass();
	Table->AddRow(TEXT("Chair"), Row);
	TestTrue(TEXT("Custom actor purchase"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Product = Shop->ConfirmPurchase(FTransform(FVector(500, 0, 100)), Error);
	TestTrue(TEXT("Configured class is spawned"), Product && Product->GetClass() == AActor::StaticClass());
	TestEqual(TEXT("Custom product charged once"), Economy->GetCapital(), 10.0f);
	TestTrue(TEXT("Final purchase uses remaining funds"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	// A finite but non-normalized rotation is invalid; no actor should be spawned and funds must return.
	FTransform Invalid(FQuat(0, 0, 0, 0), FVector::ZeroVector);
	TestNull(TEXT("Failed confirmation produces no actor"), Shop->ConfirmPurchase(Invalid, Error));
	TestFalse(TEXT("Failed confirmation cancels reservation"), Shop->HasPendingPurchase());
	TestFalse(TEXT("Failure includes a user-facing error"), Error.IsEmpty());
	TestEqual(TEXT("Failed confirmation refunds"), Economy->GetCapital(), 10.0f);
	TestTrue(TEXT("Pending purchase before shutdown"), Shop->BeginPurchase(Table, TEXT("Chair"), Error));
	Shop->Deinitialize();
	TestEqual(TEXT("Shutdown refunds pending funds"), Economy->GetCapital(), 10.0f);
	TestFalse(TEXT("Shutdown refund cannot repeat"), Shop->CancelPurchase());
	World->DestroyWorld(false);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShopCatalogTest, "TheLastCheckIn.Shop.DesktopCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShopCatalogTest::RunTest(const FString& Parameters)
{
	auto* Table = NewObject<UDataTable>();
	Table->RowStruct = FShopProductRow::StaticStruct();
	FShopProductRow Row;
	Row.DisplayName = FText::FromString(TEXT("Test Chair"));
	Row.Price = 42;
	Table->AddRow(TEXT("Chair"), Row);
	Row.bAvailable = false;
	Table->AddRow(TEXT("HiddenChair"), Row);
	Row.bAvailable = true;
	Row.Category = EShopProductCategory::Tool;
	Row.DisplayName = FText::FromString(TEXT("Test Tool"));
	Row.Price = 7;
	Table->AddRow(TEXT("Tool"), Row);
	TGuardValue<TSoftObjectPtr<UDataTable>> Catalog(GetMutableDefault<UShopSettings>()->ProductTable, TSoftObjectPtr<UDataTable>(Table));
	auto Class = LoadClass<UDesktopSurfaceWidget>(nullptr, TEXT("/Game/Desktop/UI/WBP_Desktop.WBP_Desktop_C"));
	if (!TestNotNull(TEXT("Desktop class loads"), Class)) return false;
	auto* Widget = CreateWidget<UDesktopSurfaceWidget>(GEditor->GetEditorWorldContext().World(), Class);
	const auto Slate = Widget->TakeWidget();
	for (int32 Category = 0; Category < 2; ++Category)
	{
		Widget->SelectPage(1, Category);
		int32 Cards = 0;
		bool bName = false, bPrice = false;
		Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
		{
			if (auto* Button = Cast<UDesktopActionButton>(Child)) if (Button->Action >= 3000) ++Cards;
			if (auto* Text = Cast<UTextBlock>(Child))
			{
				bName |= Text->GetText().ToString() == (Category == 0 ? TEXT("Test Chair") : TEXT("Test Tool"));
				bPrice |= Text->GetText().ToString() == (Category == 0 ? TEXT("G 42") : TEXT("G 7"));
			}
		});
		TestEqual(TEXT("Only available products of this category appear"), Cards, 1);
		TestTrue(TEXT("Catalog name is displayed"), bName);
		TestTrue(TEXT("Catalog price is displayed"), bPrice);
	}
	Widget->ReleaseSlateResources(true);
	return true;
}
#endif
#endif
