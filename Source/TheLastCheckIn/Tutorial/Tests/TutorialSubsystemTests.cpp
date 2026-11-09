#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Tutorial/TutorialSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialProgressionTest, "TheLastCheckIn.Tutorial.Progression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTutorialProgressionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Values);
	auto* Tutorial = World->GetSubsystem<UTutorialSubsystem>();
	auto* Phase = World->GetSubsystem<UHotelPhaseSubsystem>();
	auto* Clock = World->GetSubsystem<UGameClockSubsystem>();

	TestFalse(TEXT("Inactive before run starts"), Tutorial->IsTutorialActive());
	TestFalse(TEXT("Cannot complete before start"), Tutorial->CompleteStep(ETutorialStep::CleanRoom));
	Phase->StartRun();
	TestEqual(TEXT("New run starts at cleaning"), Tutorial->GetCurrentStep(), ETutorialStep::CleanRoom);
	TestFalse(TEXT("Cannot skip to CCTV"), Tutorial->CompleteStep(ETutorialStep::ViewCCTV));

	const ETutorialStep Steps[] = {
		ETutorialStep::CleanRoom, ETutorialStep::UnlockRoom205, ETutorialStep::CheckInGuest,
		ETutorialStep::AnomalyPresentation, ETutorialStep::ReadBoardMemo,
		ETutorialStep::ViewCCTV, ETutorialStep::Sleep
	};
	for (ETutorialStep Step : Steps)
	{
		TestEqual(TEXT("Expected objective order"), Tutorial->GetCurrentStep(), Step);
		TestTrue(TEXT("Successful objective advances"), Tutorial->CompleteStep(Step));
		TestFalse(TEXT("Duplicate notification ignored"), Tutorial->CompleteStep(Step));
	}
	TestTrue(TEXT("All objectives complete"), Tutorial->IsTutorialCompleted());
	TestFalse(TEXT("Completed tutorial is inactive"), Tutorial->IsTutorialActive());

	const auto AdvanceDay = [&]()
	{
		const double Previous = Clock->GetTotalGameSeconds();
		Clock->StartClock(86400.0, 60.0);
		Clock->OnTimeAdvanced.Broadcast(Previous, 86400.0);
		Phase->RequestSleepSettlement();
		Phase->ConfirmSettlement();
	};
	AdvanceDay();
	TestEqual(TEXT("Second day reached"), Phase->GetCurrentDay(), 2);
	TestTrue(TEXT("Completion survives next morning"), Tutorial->IsTutorialCompleted());
	Phase->StartRun();
	TestEqual(TEXT("New run resets completed tutorial"), Tutorial->GetCurrentStep(), ETutorialStep::CleanRoom);
	AdvanceDay();
	TestFalse(TEXT("Leaving day one does not falsely complete tutorial"), Tutorial->IsTutorialCompleted());
	TestFalse(TEXT("Unfinished tutorial inactive on day two"), Tutorial->IsTutorialActive());
	TestFalse(TEXT("Late notification ignored"), Tutorial->CompleteStep(ETutorialStep::CleanRoom));

	World->DestroyWorld(false);
	return true;
}

#endif
