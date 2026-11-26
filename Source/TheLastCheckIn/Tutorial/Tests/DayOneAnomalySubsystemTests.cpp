#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Tutorial/DayOneAnomalySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDayOneAnomalyScheduleTest, "TheLastCheckIn.Tutorial.DayOneAnomalySchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDayOneAnomalyScheduleTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Clock = World->GetSubsystem<UGameClockSubsystem>();
	auto* Phase = World->GetSubsystem<UHotelPhaseSubsystem>();
	auto* Schedule = World->GetSubsystem<UDayOneAnomalySubsystem>();
	using Id = EDayOneAnomaly;
	using State = EDayOneAnomalyState;
	auto Advance = [Clock](double Now)
	{
		const double Before = Clock->GetTotalGameSeconds();
		Clock->StartClock(Now);
		Clock->OnTimeAdvanced.Broadcast(Before, Now);
	};
	Clock->StartClock(GameTime::HoursToSeconds(6));
	TestFalse(TEXT("Not armed before day event"), Schedule->IsScheduleActive());
	Phase->OnDayStarted.Broadcast(1);
	const double Start = Schedule->GetPlannedFlickerStartSeconds();
	TestTrue(TEXT("Lobby start within 18:00-18:05"), Start >= 64800 && Start <= 65100);
	Phase->OnDayStarted.Broadcast(1);
	TestEqual(TEXT("Duplicate day notification does not reroll"), Schedule->GetPlannedFlickerStartSeconds(), Start);
	Advance(Start - 1);
	TestEqual(TEXT("Wait before scheduled start"), Schedule->GetProgress(Id::LobbyFlicker).State, State::Waiting);
	const double ActualStart = FMath::Min(Start + 0.5, 65100.0);
	Advance(ActualStart);
	TestEqual(TEXT("Starts without player location or CCTV"), Schedule->GetProgress(Id::LobbyFlicker).State, State::Active);
	TestEqual(TEXT("Full duration despite discrete clock updates"), Schedule->GetProgress(Id::LobbyFlicker).EndAtSeconds - ActualStart, 3600.0);
	Advance(ActualStart + 3599);
	TestEqual(TEXT("Still active until an hour"), Schedule->GetProgress(Id::LobbyFlicker).State, State::Active);
	Advance(ActualStart + 3600);
	TestEqual(TEXT("Ends after one game hour"), Schedule->GetProgress(Id::LobbyFlicker).State, State::Finished);
	Advance(72000);
	TestEqual(TEXT("Shadow waits for CCTV"), Schedule->GetProgress(Id::ShadowFigure).State, State::Waiting);
	Schedule->SetCCTVViewing(true);
	TestEqual(TEXT("Shadow starts when CCTV opens in window"), Schedule->GetProgress(Id::ShadowFigure).State, State::Active);
	Schedule->SetCCTVViewing(false);
	TestEqual(TEXT("Shadow ends when leaving CCTV"), Schedule->GetProgress(Id::ShadowFigure).State, State::Finished);
	Schedule->SetCCTVViewing(true);
	TestEqual(TEXT("Reopening does not replay shadow"), Schedule->GetProgress(Id::ShadowFigure).State, State::Finished);
	Schedule->SetCCTVViewing(false);
	Advance(79200);
	TestEqual(TEXT("Knock waits for context"), Schedule->GetProgress(Id::DoorKnock).State, State::Waiting);
	Schedule->SetPlayerInStaffRoom(true);
	TestEqual(TEXT("Staff room alone triggers knock"), Schedule->GetProgress(Id::DoorKnock).State, State::Active);
	Phase->OnPhaseChanged.Broadcast(EHotelPhase::Night, EHotelPhase::Settlement);
	TestFalse(TEXT("Settlement disables schedule"), Schedule->IsScheduleActive());
	TestEqual(TEXT("Settlement cleans up active knock"), Schedule->GetProgress(Id::DoorKnock).State, State::Finished);
	Phase->OnDayStarted.Broadcast(2);
	Advance(86400 + 64800);
	TestFalse(TEXT("No day-two schedule"), Schedule->IsScheduleActive());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);

	// A time skip must not start and immediately end effects that were never seen.
	World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	Clock = World->GetSubsystem<UGameClockSubsystem>();
	Phase = World->GetSubsystem<UHotelPhaseSubsystem>();
	Schedule = World->GetSubsystem<UDayOneAnomalySubsystem>();
	Clock->StartClock(21600);
	Phase->OnDayStarted.Broadcast(1);
	Clock->StartClock(84600);
	Clock->OnTimeAdvanced.Broadcast(21600, 84600);
	for (Id Presentation : {Id::LobbyFlicker, Id::ShadowFigure, Id::DoorKnock})
		TestEqual(TEXT("Missed window is skipped"), Schedule->GetProgress(Presentation).State, State::Skipped);
	Schedule->SetCCTVViewing(true);
	TestEqual(TEXT("Late CCTV does not replay"), Schedule->GetProgress(Id::ShadowFigure).State, State::Skipped);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
