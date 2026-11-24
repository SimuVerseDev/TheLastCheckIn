#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/RectLight.h"
#include "Components/LightComponent.h"
#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Night/Anomaly/Light/AnomalyLightComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRectLightBlackoutTest, "TheLastCheckIn.Night.RectLightBlackout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRectLightBlackoutTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor, true);
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// Initialize actor lifecycle so Destroy() routes EndPlay as it does during gameplay.
	World->InitializeActorsForPlay(FURL());
	auto* On = World->SpawnActor<ARectLight>();
	auto* Off = World->SpawnActor<ARectLight>();
	auto* Managed = World->SpawnActor<ARectLight>();
	On->GetLightComponent()->SetIntensity(6.1217f);
	Off->GetLightComponent()->SetVisibility(false);
	auto* Anomaly = World->SpawnActor<ALightAnomaly>();
	auto* Existing = NewObject<UAnomalyLightComponent>(Managed);
	Managed->AddInstanceComponent(Existing);
	Existing->Anomaly = Anomaly;
	Existing->ZoneId = TEXT("Lobby");
	Existing->RegisterComponent();
	On->DispatchBeginPlay();
	Off->DispatchBeginPlay();
	Managed->DispatchBeginPlay();
	Anomaly->DispatchBeginPlay();
	TestNotNull(TEXT("Standalone RectLight automatically connected"), On->FindComponentByClass<UAnomalyLightComponent>());
	TArray<UAnomalyLightComponent*> ManagedOutputs;
	Managed->GetComponents(ManagedOutputs);
	TestEqual(TEXT("Existing zone controller not duplicated"), ManagedOutputs.Num(), 1);
	TestEqual(TEXT("Existing zone preserved"), Existing->ZoneId, FName(TEXT("Lobby")));

	Anomaly->OnLightingChanged.Broadcast(EHotelLightMode::Flickering, TEXT("Lobby"));
	TestTrue(TEXT("Unzoned fill light does not flicker with a local zone"), On->GetLightComponent()->IsVisible());
	TestFalse(TEXT("Zone-managed RectLight still follows its flicker zone"), Managed->GetLightComponent()->IsVisible());
	// Lights loaded after the anomaly actor's BeginPlay are connected at the blackout transition too.
	auto* Late = World->SpawnActor<ARectLight>();
	Late->DispatchBeginPlay();
	Anomaly->AAnomalyBase::StartAnomaly();
	TestTrue(TEXT("Escalate to blackout"), Anomaly->Escalate());
	TestFalse(TEXT("Standalone RectLight off during blackout"), On->GetLightComponent()->IsVisible());
	TestFalse(TEXT("Late RectLight off during blackout"), Late->GetLightComponent()->IsVisible());
	TestFalse(TEXT("Zone RectLight off during blackout"), Managed->GetLightComponent()->IsVisible());
	TestTrue(TEXT("Breaker interaction restores power"), Anomaly->TryRestoreFromBreaker());
	TestTrue(TEXT("RectLight restored on resolution"), On->GetLightComponent()->IsVisible());
	TestTrue(TEXT("Late RectLight restored"), Late->GetLightComponent()->IsVisible());
	TestTrue(TEXT("Flicker to blackout retains original visibility"), Managed->GetLightComponent()->IsVisible());
	TestFalse(TEXT("Originally disabled RectLight remains disabled"), Off->GetLightComponent()->IsVisible());
	TestEqual(TEXT("Authored intensity unchanged"), On->GetLightComponent()->Intensity, 6.1217f);

	Anomaly->AAnomalyBase::StartAnomaly();
	Anomaly->Escalate();
	Late->Destroy();
	TestTrue(TEXT("Failed anomaly restores lighting too"), Anomaly->Fail());
	TestTrue(TEXT("RectLight restored after failure"), On->GetLightComponent()->IsVisible());
	Anomaly->AAnomalyBase::StartAnomaly();
	Anomaly->Escalate();
	Anomaly->Destroy();
	TestTrue(TEXT("Removing anomaly restores RectLight"), On->GetLightComponent()->IsVisible());
	TestNull(TEXT("Temporary controller removed"), On->FindComponentByClass<UAnomalyLightComponent>());
	TestNotNull(TEXT("Authored zone controller preserved"), Managed->FindComponentByClass<UAnomalyLightComponent>());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
