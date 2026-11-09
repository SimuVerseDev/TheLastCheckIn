#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/GameFlow/HotelPhaseSettings.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Night/Settlement/DailySettlementSubsystem.h"
#include "Night/Settlement/SettlementWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

namespace
{
	struct FNightTestWorld
	{
		UWorld* World;
		FNightTestWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
		}
		~FNightTestWorld() { World->DestroyWorld(false); }

		void SetTime(double Seconds)
		{
			UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>();
			const double Previous = Clock->GetTotalGameSeconds();
			Clock->StartClock(Seconds, 60.0);
			Clock->OnTimeAdvanced.Broadcast(Previous, Seconds);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightSettlementFlowTest, "TheLastCheckIn.Night.SettlementFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightSettlementFlowTest::RunTest(const FString& Parameters)
{
	FNightTestWorld Fixture;
	auto* Phase = Fixture.World->GetSubsystem<UHotelPhaseSubsystem>();
	auto* Clock = Fixture.World->GetSubsystem<UGameClockSubsystem>();
	auto* Economy = Fixture.World->GetSubsystem<UHotelEconomySubsystem>();
	auto* Settlement = Fixture.World->GetSubsystem<UDailySettlementSubsystem>();
	Phase->StartRun();
	Fixture.SetTime(86399.0);
	TestFalse(TEXT("23:59:59 cannot sleep"), Phase->CanSleep());
	Phase->RequestSleepSettlement();
	TestTrue(TEXT("Early request leaves clock running"), Clock->IsClockRunning());
	Fixture.SetTime(86400.0);
	TestTrue(TEXT("00:00 can sleep"), Phase->CanSleep());
	TestTrue(TEXT("Midnight does not stop clock"), Clock->IsClockRunning());
	Fixture.SetTime(86400.0 + 4 * 3600 + 59 * 60 + 59);
	TestTrue(TEXT("04:59:59 can sleep"), Phase->CanSleep());
	Economy->AddIncome(150.0f);
	Settlement->RecordReputation(10, 12);
	FDailyAnomalyResult Anomaly;
	Anomaly.Id = TEXT("HallLight");
	Anomaly.Status = EDailyAnomalyStatus::Unresolved;
	Settlement->RecordAnomaly(Anomaly);
	Anomaly.Status = EDailyAnomalyStatus::Resolved;
	Settlement->RecordAnomaly(Anomaly);
	Settlement->SetPendingEndReason(EDayEndReason::Sleep);
	Phase->RequestSleepSettlement();
	TestEqual(TEXT("Sleep enters settlement"), Phase->GetCurrentPhase(), EHotelPhase::Settlement);
	TestFalse(TEXT("Clock stopped in settlement"), Clock->IsClockRunning());
	TestEqual(TEXT("Snapshot uses operating day"), Settlement->GetLastResult().Day, 1);
	TestEqual(TEXT("Income captured"), Settlement->GetLastResult().Income, 150.0f);
	TestEqual(TEXT("No duplicate anomaly IDs"), Settlement->GetLastResult().Anomalies.Num(), 1);
	TestEqual(TEXT("Resolved state captured"), Settlement->GetLastResult().Anomalies[0].Status, EDailyAnomalyStatus::Resolved);
	Phase->RequestSleepSettlement();
	TestEqual(TEXT("Repeated sleep does not advance day"), Phase->GetCurrentDay(), 1);
	Phase->ConfirmSettlement();
	Phase->ConfirmSettlement();
	TestEqual(TEXT("Confirm advances exactly once"), Phase->GetCurrentDay(), 2);
	TestEqual(TEXT("Next morning is 06:00"), Clock->GetHour(), 6);
	TestEqual(TEXT("Next morning minute is zero"), Clock->GetMinute(), 0);
	TestTrue(TEXT("Next morning clock running"), Clock->IsClockRunning());
	TestEqual(TEXT("Today's income reset"), Economy->GetTodayIncome(), 0.0f);
	TestEqual(TEXT("Previous snapshot retained"), Settlement->GetLastResult().Income, 150.0f);
	TestEqual(TEXT("Income was not awarded twice"), Economy->GetCapital(), 150.0f);
	Fixture.SetTime(2 * 86400.0 + 5 * 3600);
	TestFalse(TEXT("05:00 cannot sleep"), Phase->CanSleep());
	TestEqual(TEXT("05:00 automatic settlement preserved"), Phase->GetCurrentPhase(), EHotelPhase::Settlement);
	TestEqual(TEXT("Automatic reason restored on new day"), Settlement->GetLastResult().EndReason, EDayEndReason::Automatic);
	Phase->ConfirmSettlement();
	TestEqual(TEXT("Automatic settlement advances to next day"), Phase->GetCurrentDay(), 3);
	{
		TGuardValue<int32> LastDaySetting(GetMutableDefault<UHotelPhaseSettings>()->MaxDays, 3);
		Fixture.SetTime(3 * 86400.0 + 5 * 3600);
		Phase->ConfirmSettlement();
		Phase->ConfirmSettlement();
		TestEqual(TEXT("Final day never creates an extra day"), Phase->GetCurrentDay(), 3);
		TestFalse(TEXT("Final day clock remains stopped"), Clock->IsClockRunning());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightNativeWidgetTest, "TheLastCheckIn.Night.NativeSettlementWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightNativeWidgetTest::RunTest(const FString& Parameters)
{
	FNightTestWorld Fixture;
	APlayerController* PC = Fixture.World->SpawnActor<APlayerController>();
	Fixture.World->AddController(PC);
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	PC->Player = Player;
	Player->PlayerController = PC;
	USettlementWidget* Widget = NewObject<USettlementWidget>(Fixture.World);
	Widget->SetPlayerContext(FLocalPlayerContext(Player, Fixture.World));
	Widget->Initialize();
	FDailySettlementData Result;
	Result.Day = 1;
	Result.Income = 150.0f;
	Widget->SetResult(Result, false);
	// Keep the Slate tree alive as a viewport would; otherwise NativeDestruct unbinds the button.
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("NextDayButton")));
	UTextBlock* Income = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("IncomeText")));
	if (!TestNotNull(TEXT("Native next-day button exists without a Blueprint"), Button)
		|| !TestNotNull(TEXT("Native income field exists"), Income)) return false;
	TestEqual(TEXT("Snapshot rendered"), Income->GetText().ToString(), FText::AsNumber(150.0f).ToString());
	int32 ConfirmCount = 0;
	Widget->OnConfirmed.AddLambda([&ConfirmCount]() { ++ConfirmCount; });
	Button->OnClicked.Broadcast();
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Double-click confirms once"), ConfirmCount, 1);
	TestFalse(TEXT("Confirmed button disabled"), Button->GetIsEnabled());
	Widget->OnConfirmed.Clear();
	Widget->ReleaseSlateResources(true);
	return true;
}

#endif
