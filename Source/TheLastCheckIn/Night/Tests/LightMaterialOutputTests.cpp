#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Night/Anomaly/Light/AnomalyLightComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLightMaterialOutputTest, "TheLastCheckIn.Night.LightMaterialOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLightMaterialOutputTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Values);
	ALightAnomaly* Anomaly = World->SpawnActor<ALightAnomaly>();
	UStaticMesh* TestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Mesh with a material slot"), TestMesh))
	{
		World->DestroyWorld(false);
		return false;
	}

	struct FMaterialCase { const TCHAR* Path; const TCHAR* Parameter; };
	const FMaterialCase Cases[] = {
		{ TEXT("/Game/Design/Assets/Props/BrassChandelier/MI_BrassChandelier.MI_BrassChandelier"), TEXT("Emissive_Intensity") },
		{ TEXT("/Game/Fab/Wall_Sconce_-_Arandela/wall_sconce_arandela/Materials/Glass.Glass"), TEXT("EmissiveStrength") }
	};
	for (const FMaterialCase& Case : Cases)
	{
		UMaterialInterface* Asset = LoadObject<UMaterialInterface>(nullptr, Case.Path);
		if (!TestNotNull(Case.Path, Asset)) continue;
		const FName Parameter(Case.Parameter);
		float AssetValue = 0.0f;
		if (!TestTrue(TEXT("Lamp material exposes its emissive scalar"),
			Asset->GetScalarParameterValue(FMaterialParameterInfo(Parameter), AssetValue))) continue;

		AActor* Lamp = World->SpawnActor<AActor>();
		auto* Mesh = NewObject<UStaticMeshComponent>(Lamp);
		Mesh->SetStaticMesh(TestMesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lamp->SetRootComponent(Mesh);
		Lamp->AddInstanceComponent(Mesh);
		Mesh->RegisterComponent();
		auto* Source = UMaterialInstanceDynamic::Create(Asset, Lamp);
		Source->SetScalarParameterValue(Parameter, 7.5f);
		Mesh->SetMaterial(0, Source);

		auto AddLight = [Lamp, Mesh](bool bVisible)
		{
			auto* Light = NewObject<UPointLightComponent>(Lamp);
			Lamp->AddInstanceComponent(Light);
			Light->SetupAttachment(Mesh);
			Light->SetVisibility(bVisible);
			Light->RegisterComponent();
			return Light;
		};
		UPointLightComponent* OnLight = AddLight(true);
		UPointLightComponent* OffLight = AddLight(false);
		auto* Output = NewObject<UAnomalyLightComponent>(Lamp);
		Lamp->AddInstanceComponent(Output);
		Output->AssignToZone(Lamp, Anomaly, TEXT("TestZone"));
		Output->RegisterComponent();
		Lamp->DispatchBeginPlay();

		auto ReadScalar = [Parameter](UMaterialInterface* Material)
		{
			float Value = -1.0f;
			if (Material) Material->GetScalarParameterValue(FMaterialParameterInfo(Parameter), Value);
			return Value;
		};

		Anomaly->OnLightingChanged.Broadcast(EHotelLightMode::Flickering, TEXT("OtherZone"));
		TestTrue(TEXT("Other zone keeps its material"), Mesh->GetMaterial(0) == Source);
		Anomaly->OnLightingChanged.Broadcast(EHotelLightMode::Flickering, TEXT("TestZone"));
		TestEqual(TEXT("Flicker off also disables emission"), ReadScalar(Mesh->GetMaterial(0)), 0.0f);
		TestTrue(TEXT("Each lamp receives a separate dynamic instance"), Mesh->GetMaterial(0) != Source);
		TestEqual(TEXT("Shared source stays unchanged"), ReadScalar(Source), 7.5f);

		Anomaly->OnLightingChanged.Broadcast(EHotelLightMode::Blackout, NAME_None);
		TestFalse(TEXT("Blackout hides the light"), OnLight->IsVisible());
		TestEqual(TEXT("Blackout disables emission"), ReadScalar(Mesh->GetMaterial(0)), 0.0f);
		Anomaly->OnLightingChanged.Broadcast(EHotelLightMode::Normal, NAME_None);
		TestTrue(TEXT("Resolution restores original material instance"), Mesh->GetMaterial(0) == Source);
		TestEqual(TEXT("Flicker to blackout preserves original brightness"), ReadScalar(Mesh->GetMaterial(0)), 7.5f);
		TestTrue(TEXT("Resolution restores originally visible light"), OnLight->IsVisible());
		TestFalse(TEXT("Originally disabled light remains disabled"), OffLight->IsVisible());

		Anomaly->OnLightingChanged.Broadcast(EHotelLightMode::Blackout, NAME_None);
		TestEqual(TEXT("A second blackout also disables emission"), ReadScalar(Mesh->GetMaterial(0)), 0.0f);
		Output->ReleaseFromZone(Lamp);
		TestTrue(TEXT("Zone release restores original material"), Mesh->GetMaterial(0) == Source);
		TestTrue(TEXT("Zone release restores light visibility"), OnLight->IsVisible());
		float AssetAfter = 0.0f;
		Asset->GetScalarParameterValue(FMaterialParameterInfo(Parameter), AssetAfter);
		TestEqual(TEXT("Asset brightness was not modified"), AssetAfter, AssetValue);
		Lamp->Destroy();
	}
	World->DestroyWorld(false);
	return true;
}

#endif
