#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Components/SceneComponent.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Core/Economy/HotelEconomySettings.h"
#include "Housekeeping/Core/Room/HKRoomVolume.h"
#include "Housekeeping/Objects/Stain/HKStainDirtComponent.h"
#include "Rooms/RoomActor.h"
#include "Tutorial/TutorialSubsystem.h"
#include "Tutorial/UI/TutorialQuestWidget.h"
#include "CCTV/CCTVMonitor.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialProgressionTest, "TheLastCheckIn.Tutorial.Progression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTutorialProgressionTest::RunTest(const FString& Parameters)
{
    TGuardValue<float> StartingCapital(GetMutableDefault<UHotelEconomySettings>()->StartingCapital, 100.0f);
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    auto* Tutorial = World->GetSubsystem<UTutorialSubsystem>();
    auto* Phase = World->GetSubsystem<UHotelPhaseSubsystem>();
    auto* Economy = World->GetSubsystem<UHotelEconomySubsystem>();
    auto* Clock = World->GetSubsystem<UGameClockSubsystem>();
    TArray<ARoomActor*> Rooms;
    TArray<AHKRoomVolume*> Volumes;
    TArray<UHKStainDirtComponent*> Stains;
    UDataTable* RoomTypes = NewObject<UDataTable>(World);
    RoomTypes->RowStruct = FRoomTypeRow::StaticStruct();
    RoomTypes->AddRow(TEXT("Test"), FRoomTypeRow());
    for (int32 Index = 0; Index < 5; ++Index)
    {
        auto* Room = World->SpawnActor<ARoomActor>();
        FindFProperty<FIntProperty>(ARoomActor::StaticClass(), TEXT("RoomNumber"))->SetPropertyValue_InContainer(Room, 201 + Index);
        auto* Handle = FindFProperty<FStructProperty>(ARoomActor::StaticClass(), TEXT("RoomTypeRow"))->ContainerPtrToValuePtr<FDataTableRowHandle>(Room);
        Handle->DataTable = RoomTypes; Handle->RowName = TEXT("Test");
        FindFProperty<FObjectPropertyBase>(ARoomActor::StaticClass(), TEXT("ArrivalTarget"))->SetObjectPropertyValue_InContainer(Room, Room);
        Rooms.Add(Room);
        if (Index == 4) continue;
        Room->UnlockRoom();
        auto* Volume = World->SpawnActor<AHKRoomVolume>(FVector(Index * 2000.0, 0, 0), FRotator::ZeroRotator);
        FindFProperty<FObjectPropertyBase>(AHKRoomVolume::StaticClass(), TEXT("LinkedRoom"))->SetObjectPropertyValue_InContainer(Volume, Room);
        for (const FName Name : {FName("MinTrash"), FName("MaxTrash"), FName("MinStains"), FName("MaxStains")})
            FindFProperty<FIntProperty>(AHKRoomVolume::StaticClass(), Name)->SetPropertyValue_InContainer(Volume, 0);
        FindFProperty<FBoolProperty>(AHKRoomVolume::StaticClass(), TEXT("bShowDebug"))->SetPropertyValue_InContainer(Volume, false);
        AActor* DirtActor = World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(DirtActor);
        DirtActor->SetRootComponent(Root); Root->RegisterComponent();
        DirtActor->SetActorLocation(FVector(Index * 2000.0, 0, 0));
        auto* Stain = NewObject<UHKStainDirtComponent>(DirtActor);
        FindFProperty<FFloatProperty>(UHKStainDirtComponent::StaticClass(), TEXT("WipeDistanceToClean"))->SetPropertyValue_InContainer(Stain, 100.0f);
        DirtActor->AddInstanceComponent(Stain); Stain->RegisterComponent();
        DirtActor->DispatchBeginPlay();
        Volumes.Add(Volume); Stains.Add(Stain);
    }
    TestFalse(TEXT("Inactive before start"), Tutorial->IsTutorialActive());
    Phase->StartRun();
    // Activate newly pending timers before advancing their first evaluation interval.
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.0f);
    TestEqual(TEXT("Starting funds are not lodging income"), Economy->GetTodayIncome(), 0.0f);
    TestFalse(TEXT("Cannot skip cleaning"), Tutorial->CompleteStep(ETutorialStep::CleanRoom));
    for (int32 I = 0; I < 4; ++I)
    {
        TestTrue(TEXT("Initial room is dirty"), Volumes[I]->IsDirty());
        Volumes[I]->OnCleanlinessChanged.Broadcast(Volumes[I], 100.0f);
        TestEqual(TEXT("Dirty event is not accepted"), Tutorial->GetCleanedRoomCount(), I);
        // Exercise the existing cleanliness evaluator after wiping the actual dirt source.
        Stains[I]->AddWipe(49.0f);
        ++GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        TestEqual(TEXT("Cleanliness below 50 does not count"), Tutorial->GetCleanedRoomCount(), I);
        TestTrue(TEXT("Room has reached 49 cleanliness"), FMath::IsNearlyEqual(Volumes[I]->GetCleanliness(), 49.0f));
        Stains[I]->AddWipe(1.0f);
        ++GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        TestTrue(TEXT("Room has reached 50 cleanliness"), FMath::IsNearlyEqual(Volumes[I]->GetCleanliness(), 50.0f));
        TestEqual(TEXT("50 cleanliness completes the quest room"), Tutorial->GetCleanedRoomCount(), I + 1);
        TestTrue(TEXT("Quest completion preserves dirty state"), Volumes[I]->IsDirty());
        TestTrue(TEXT("Quest completion preserves remaining stain"), Stains[I]->GetAmount() > 0.0f);
        Volumes[I]->OnCleanlinessChanged.Broadcast(Volumes[I], Volumes[I]->GetCleanliness());
        TestEqual(TEXT("Repeated cleaning event ignored"), Tutorial->GetCleanedRoomCount(), I + 1);
        Stains[I]->AddWipe(1000.0f);
        ++GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        TestFalse(TEXT("Full cleaning still clears the actual room"), Volumes[I]->IsDirty());
        TestEqual(TEXT("Full cleaning does not count twice"), Tutorial->GetCleanedRoomCount(), I + 1);
    }
    TestEqual(TEXT("Four rooms reveal paid unlock objective"), Tutorial->GetCurrentStep(), ETutorialStep::UnlockRoom205);
    TestFalse(TEXT("Cannot bypass payment"), Tutorial->CompleteStep(ETutorialStep::UnlockRoom205));
    TestTrue(TEXT("Spend initial capital"), Economy->TrySpend(100.0f));
    TestFalse(TEXT("Insufficient funds cannot unlock"), Rooms[4]->TryPurchaseUnlock());
    TestFalse(TEXT("Failed purchase leaves room locked"), Rooms[4]->IsUnlocked());
    Economy->AddIncome(100.0f);
    TestTrue(TEXT("Purchase succeeds"), Rooms[4]->TryPurchaseUnlock());
    TestEqual(TEXT("Purchase deducted capital"), Economy->GetCapital(), 0.0f);
    TestEqual(TEXT("Purchase preserves gross income"), Economy->GetTodayIncome(), 100.0f);
    TestFalse(TEXT("No duplicate purchase"), Rooms[4]->TryPurchaseUnlock());
    TestEqual(TEXT("Payment reveals five-team objective"), Tutorial->GetCurrentStep(), ETutorialStep::CheckInGuest);
    for (int32 I = 0; I < 5; ++I)
    {
        const FGuid StayId = FGuid::NewGuid();
        TestTrue(TEXT("Reserve room"), Rooms[I]->TryReserve(StayId));
        TestEqual(TEXT("Reservation alone does not count"), Tutorial->GetCheckedInTeamCount(), I);
        TestTrue(TEXT("Actual occupancy counts one team"), Rooms[I]->MarkOccupied(StayId));
        TestFalse(TEXT("Repeated occupancy rejected"), Rooms[I]->MarkOccupied(StayId));
        Rooms[I]->OnOccupied.Broadcast(Rooms[I], StayId);
        TestEqual(TEXT("Unique stays counted once"), Tutorial->GetCheckedInTeamCount(), I + 1);
        if (I < 4) TestEqual(TEXT("Still waiting for five teams"), Tutorial->GetCurrentStep(), ETutorialStep::CheckInGuest);
    }
    TestEqual(TEXT("Five teams advance to CCTV"), Tutorial->GetCurrentStep(), ETutorialStep::ViewCCTV);
    TestFalse(TEXT("Cannot skip CCTV duration"), Tutorial->CompleteStep(ETutorialStep::ViewCCTV));
    auto* Monitor = World->SpawnActor<ACCTVMonitor>();
    auto* PC = World->SpawnActor<APlayerController>();
    PC->Player = NewObject<ULocalPlayer>(GEngine);
    Cast<ULocalPlayer>(PC->Player)->PlayerController = PC;
    UTutorialQuestWidget* QuestWidget = NewObject<UTutorialQuestWidget>(World);
    QuestWidget->SetPlayerContext(FLocalPlayerContext(Cast<ULocalPlayer>(PC->Player), World));
    QuestWidget->Initialize();
    const TSharedRef<SWidget> SlateQuest = QuestWidget->TakeWidget();
    TestEqual(TEXT("Active quest is visible"), QuestWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
    // This synthetic world has not initialized actors through BeginPlay.
    if (!PC->PlayerCameraManager)
    {
        PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
        PC->PlayerCameraManager->InitializeFor(PC);
    }
    auto* Camera = World->SpawnActor<ACameraActor>();
    auto* OtherCamera = World->SpawnActor<ACameraActor>();
    // Exercise the viewing state without requiring UMG animations or a viewport.
    Monitor->ViewingPC = PC;
    Monitor->Cameras.Add(Camera);
    Monitor->Cameras.Add(OtherCamera);
    Monitor->bPadOpen = true;
    Monitor->bTransitioning = true;
    Monitor->ApplyCurrentCamera();
    const auto Advance = [Clock](double Seconds)
    {
        const double Before = Clock->GetTotalGameSeconds();
        Clock->StartClock(Before + Seconds, 20.0);
        Clock->OnTimeAdvanced.Broadcast(Before, Before + Seconds);
    };
    Advance(600.0);
    TestEqual(TEXT("Opening animation does not count"), Tutorial->GetCCTVViewingMinutes(), 0.0f);
    Monitor->bTransitioning = false;
    Monitor->ApplyCurrentCamera();
    TestTrue(TEXT("Camera is actually viewed"), Monitor->IsViewingCCTV());
    Advance(1800.0);
    TestEqual(TEXT("Half hour progress"), Tutorial->GetCCTVViewingMinutes(), 30.0f);
    Monitor->SwitchCamera(1);
    TestEqual(TEXT("Switching camera preserves progress"), Tutorial->GetCCTVViewingMinutes(), 30.0f);
    Advance(1799.0);
    TestEqual(TEXT("3599 seconds is not enough"), Tutorial->GetCurrentStep(), ETutorialStep::ViewCCTV);
    Monitor->ClosePad();
    Monitor->bPadOpen = false;
    TestEqual(TEXT("Closing resets continuous progress immediately"), Tutorial->GetCCTVViewingMinutes(), 0.0f);
    Advance(60.0);
    TestEqual(TEXT("Closed time does not count"), Tutorial->GetCCTVViewingMinutes(), 0.0f);
    Monitor->bPadOpen = true;
    Monitor->ApplyCurrentCamera();
    Advance(1800.0);
    PC->SetViewTargetWithBlend(Camera, 0.0f); // selected camera is OtherCamera
    Advance(1.0);
    TestEqual(TEXT("Losing camera view resets progress"), Tutorial->GetCCTVViewingMinutes(), 0.0f);
    Monitor->ApplyCurrentCamera();
    Advance(3599.0);
    TestEqual(TEXT("Reopened session must earn its own hour"), Tutorial->GetCurrentStep(), ETutorialStep::ViewCCTV);
    Advance(1.0);
    TestTrue(TEXT("Continuous hour completes tutorial without sleeping"), Tutorial->IsTutorialCompleted());
    TestFalse(TEXT("No active quest after CCTV completion"), Tutorial->IsTutorialActive());
    TestTrue(TEXT("Sleep instruction is shown on day one"), Tutorial->ShouldShowCompletionNotice());
    TestEqual(TEXT("Completion instruction is visible"), QuestWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Monitor->ClosePad();
    TestTrue(TEXT("Closing after success preserves completion"), Tutorial->IsTutorialCompleted());
    TestFalse(TEXT("Sleeping is no longer a required objective"), Tutorial->CompleteStep(ETutorialStep::Sleep));
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.0f);
    ++GFrameCounter;
    World->GetTimerManager().Tick(5.0f);
    TestTrue(TEXT("Completion notice remains readable for five seconds"), Tutorial->ShouldShowCompletionNotice());
    ++GFrameCounter;
    World->GetTimerManager().Tick(1.1f);
    TestTrue(TEXT("Completion notice does not expire after six seconds"), Tutorial->ShouldShowCompletionNotice());
    TestEqual(TEXT("Sleep instruction remains visible without sleeping"), QuestWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
    QuestWidget->RefreshQuest();
    TestEqual(TEXT("Refresh preserves the day-one instruction"), QuestWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Monitor->ViewingPC.Reset();
    const double Previous = Clock->GetTotalGameSeconds();
    Clock->StartClock(86400.0, 60.0); Clock->OnTimeAdvanced.Broadcast(Previous, 86400.0);
    Phase->RequestSleepSettlement();
    TestTrue(TEXT("Sleep instruction stays until the next day starts"), Tutorial->ShouldShowCompletionNotice());
    Phase->ConfirmSettlement();
    TestFalse(TEXT("Day two tutorial inactive"), Tutorial->IsTutorialActive());
    TestFalse(TEXT("Day two has no completion notice"), Tutorial->ShouldShowCompletionNotice());
    TestEqual(TEXT("Day two widget stays hidden"), QuestWidget->GetVisibility(), ESlateVisibility::Collapsed);
    QuestWidget->RefreshQuest();
    TestEqual(TEXT("Refresh cannot show the day-one instruction on day two"), QuestWidget->GetVisibility(), ESlateVisibility::Collapsed);
    auto* ExtraRoom = World->SpawnActor<ARoomActor>();
    Economy->AddIncome(100.0f);
    TestTrue(TEXT("Paid unlock remains available on day two"), ExtraRoom->TryPurchaseUnlock());
    TestEqual(TEXT("Later day purchase doesn't reactivate tutorial"), Tutorial->GetCurrentStep(), ETutorialStep::Completed);
    QuestWidget->ReleaseSlateResources(true);
    World->DestroyWorld(false);
    return true;
}
#endif
