#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Night/Anomaly/Light/LightSwitchActor.h"
#include "Night/Anomaly/Light/CircuitBreakerActor.h"
#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Night/Anomaly/Light/LightInteractionCameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLightInteractionCameraTest, "TheLastCheckIn.Night.LightInteractionCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLightInteractionCameraTest::RunTest(const FString& Parameters)
{
	// Interface Execute calls route through ProcessEvent in this isolated editor test world.
	TGuardValue<bool> AllowScriptExecution(GAllowActorScriptExecutionInEditor, true);
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* PC = World->SpawnActor<APlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine);
	auto* Pawn = World->SpawnActor<APawn>();
	PC->Possess(Pawn);
	if (!PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		PC->PlayerCameraManager->InitializeFor(PC);
	}
	PC->SetViewTarget(Pawn);
	auto* Anomaly = World->SpawnActor<ALightAnomaly>();
	auto* Switch = World->SpawnActor<ALightSwitchActor>();
	auto* Breaker = World->SpawnActor<ACircuitBreakerActor>();
	Switch->Anomaly = Anomaly;
	Switch->ZoneId = TEXT("CameraTest");
	Breaker->Anomaly = Anomaly;
	// Real lever meshes let the lighting delegate regression exercise the animation's mesh lookup.
	TMap<AActor*, UStaticMeshComponent*> TestLevers;
	TArray<UStaticMeshComponent*> BreakerLevers;
	for (AActor* Target : {static_cast<AActor*>(Switch), static_cast<AActor*>(Breaker)})
	{
		auto* Lever = NewObject<UStaticMeshComponent>(Target, TEXT("Lever_1"));
		Target->AddInstanceComponent(Lever);
		Lever->SetupAttachment(Target->GetRootComponent());
		Lever->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Target == Switch
			? TEXT("/Game/Design/Assets/switch/roomSwitch/StaticMeshes/SM_switch_button.SM_switch_button")
			: TEXT("/Game/Design/Assets/InteractiveProps/PowerPanel/StaticMeshes/PivotCorrected/PowerPanel_switch.PowerPanel_switch")));
		TestNotNull(TEXT("Lever mesh available"), Lever->GetStaticMesh().Get());
		Lever->SetRelativeRotation(FRotator(0, 0, -60));
		Lever->RegisterComponent();
		TestLevers.Add(Target, Lever);
		if (Target == Breaker)
		{
			BreakerLevers.Add(Lever);
			for (int32 Index = 2; Index <= 4; ++Index)
			{
				auto* Extra = NewObject<UStaticMeshComponent>(Target, FName(*FString::Printf(TEXT("Lever_%d"), Index)));
				Target->AddInstanceComponent(Extra);
				Extra->SetupAttachment(Target->GetRootComponent());
				Extra->SetStaticMesh(Lever->GetStaticMesh());
				Extra->SetRelativeLocation(FVector(Index * 20, 0, 0));
				Extra->SetRelativeRotation(FRotator(0, 0, -60));
				Extra->RegisterComponent();
				BreakerLevers.Add(Extra);
			}
		}
		Target->DispatchBeginPlay();
	}
	auto ClickLever = [](AActor* Target, UStaticMeshComponent* Lever)
	{
		const FVector Center = Lever->GetComponentTransform().TransformPosition(Lever->GetStaticMesh()->GetBoundingBox().GetCenter());
		return Target->FindComponentByClass<ULightInteractionCameraComponent>()->SubmitLeverRay(
			Center + FVector(0, 1000, 0), Center - FVector(0, 1000, 0));
	};

	auto PressKey = [this](AActor* Target, FKey Key)
	{
		auto* Input = Target->FindComponentByClass<UInputComponent>();
		if (!TestNotNull(TEXT("Interaction input registered"), Input)) return;
		for (const FInputKeyBinding& Binding : Input->KeyBindings)
		{
			if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed)
			{
				// Cancellation destroys the input component; keep a delegate copy for this dispatch.
				auto Delegate = Binding.KeyDelegate;
				Delegate.Execute(Key);
				return;
			}
		}
		AddError(TEXT("Expected interaction key was not bound"));
	};

	// Tick the world so timers advance across real engine frames.
	auto Advance = [World, PC](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World->Tick(LEVELTICK_All, 0.05f);
			PC->PlayerCameraManager->UpdateCamera(0.05f);
			++GFrameCounter;
		}
	};
	for (AActor* Target : {static_cast<AActor*>(Switch), static_cast<AActor*>(Breaker)})
	{
		// Bypass random zone/blackout selection to exercise both interaction entry points deterministically.
		Anomaly->AAnomalyBase::StartAnomaly();
		Anomaly->ActiveZoneId = Switch->ZoneId;
		Anomaly->LightingMode = EHotelLightMode::Flickering;
		if (Target == Breaker)
		{
			Anomaly->AAnomalyBase::Escalate();
			Anomaly->LightingMode = EHotelLightMode::Blackout;
		}
		Anomaly->OnLightingChanged.Broadcast(Anomaly->LightingMode, Anomaly->ActiveZoneId);
		Target->Tick(0.2f);
		TestTrue(TEXT("Switch starts down, breaker stays up"),
			TestLevers[Target]->GetRelativeRotation().Equals(Target == Switch ? FRotator::ZeroRotator : FRotator(0, 0, -60)));
		TestTrue(TEXT("Interaction available"), IInteractable::Execute_CanInteract(Target, Pawn));
		IInteractable::Execute_Interact(Target, Pawn);
		TestEqual(TEXT("Interaction focuses its camera"), PC->GetViewTarget(), Target);
		TestTrue(TEXT("Resolution waits for camera blend"), Anomaly->IsRunning());
		TestTrue(TEXT("Movement locked while focusing"), PC->IsMoveInputIgnored());
		TestFalse(TEXT("Repeated interaction blocked"), IInteractable::Execute_CanInteract(Target, Pawn));
		PressKey(Target, EKeys::LeftMouseButton);
		TestFalse(TEXT("Lever click during blend ignored"), ClickLever(Target, TestLevers[Target]));
		TestTrue(TEXT("Click during camera blend is ignored"), Anomaly->IsRunning());
		Advance(40);
		TestTrue(TEXT("No automatic resolution after waiting"), Anomaly->IsRunning());
		TestEqual(TEXT("Camera remains focused while waiting"), PC->GetViewTarget(), Target);
		auto* ClickSequence = Target->FindComponentByClass<ULightInteractionCameraComponent>();
		TestTrue(TEXT("Cursor visible for lever selection"), PC->bShowMouseCursor);
		TestFalse(TEXT("Empty-space click ignored"), ClickSequence->SubmitLeverRay(FVector(10000), FVector(11000)));
		if (Target == Breaker)
		{
			ClickLever(Target, BreakerLevers[1]);
			Target->Tick(0.2f);
			TestTrue(TEXT("Unset sequence allows individual operation without solving"), Anomaly->IsRunning());
			TestTrue(TEXT("Only selected lever moves"), BreakerLevers[1]->GetRelativeRotation().Equals(FRotator::ZeroRotator));
			TestTrue(TEXT("Adjacent lever stays up"), BreakerLevers[0]->GetRelativeRotation().Equals(FRotator(0, 0, -60)));
			Breaker->CorrectLeverOrder = {2, 1, 4, 3};
			Breaker->ResetLeverPuzzle();
			Target->Tick(0.2f);
			ClickLever(Target, BreakerLevers[0]);
			TestEqual(TEXT("Incorrect order resets progress"), Breaker->CurrentStep, 0);
			for (int32 Number : Breaker->CorrectLeverOrder) ClickLever(Target, BreakerLevers[Number - 1]);
		}
		else ClickLever(Target, TestLevers[Target]);
		TestEqual(TEXT("Valid lever operation resolves"), Anomaly->GetState(), EAnomalyState::Resolved);
		TestFalse(TEXT("Repeated click after completion ignored"), ClickLever(Target, TestLevers[Target]));
		Target->Tick(0.2f);
		if (Target == Switch) TestTrue(TEXT("Resolved switch is raised"), TestLevers[Target]->GetRelativeRotation().Equals(FRotator(0, 0, -60)));
		Advance(18);
		TestEqual(TEXT("View stays focused for at least 0.9 seconds after success"), PC->GetViewTarget(), Target);
		Advance(12);
		TestEqual(TEXT("Player view restored"), PC->GetViewTarget(), static_cast<AActor*>(Pawn));
		TestFalse(TEXT("Movement restored"), PC->IsMoveInputIgnored());
		TestFalse(TEXT("Look restored"), PC->IsLookInputIgnored());
		TestFalse(TEXT("Cursor restored"), PC->bShowMouseCursor);
	}

	// An assigned external camera must also return cleanly when the sequence is cancelled.
	auto* Sequence = Breaker->FindComponentByClass<ULightInteractionCameraComponent>();
	auto* ExternalCamera = World->SpawnActor<ACameraActor>();
	int32 Actions = 0;
	if (TestNotNull(TEXT("Breaker has camera sequence"), Sequence))
	{
		TestTrue(TEXT("External camera accepted"), Sequence->Start(Pawn, ExternalCamera->GetCameraComponent(),
			FSimpleDelegate::CreateLambda([&Actions]() { ++Actions; })));
		TestEqual(TEXT("External view selected"), PC->GetViewTarget(), static_cast<AActor*>(ExternalCamera));
		Advance(10);
		PressKey(Breaker, EKeys::Escape);
		Advance(25);
		TestEqual(TEXT("Cancelled action never executes"), Actions, 0);
		TestEqual(TEXT("Cancellation restores view"), PC->GetViewTarget(), static_cast<AActor*>(Pawn));
		TestFalse(TEXT("Cancellation restores movement"), PC->IsMoveInputIgnored());
		TestFalse(TEXT("Cancellation restores look"), PC->IsLookInputIgnored());
		TestTrue(TEXT("Can reenter after cancelling"), Sequence->Start(Pawn, ExternalCamera->GetCameraComponent(),
			FSimpleDelegate::CreateLambda([&Actions]() { ++Actions; })));
		PressKey(Breaker, EKeys::LeftMouseButton);
		TestEqual(TEXT("Reentry still waits for its blend"), Actions, 0);
		Advance(10);
		PressKey(Breaker, EKeys::LeftMouseButton);
		PressKey(Breaker, EKeys::LeftMouseButton);
		TestEqual(TEXT("Double click executes only once"), Actions, 1);
		Advance(30);
		TestEqual(TEXT("External camera returns after confirmation"), PC->GetViewTarget(), static_cast<AActor*>(Pawn));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
