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
        Stains[I]->AddWipe(69.0f);
        ++GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        TestEqual(TEXT("Cleanliness below 70 does not count"), Tutorial->GetCleanedRoomCount(), I);
        TestTrue(TEXT("Room has reached 69 cleanliness"), FMath::IsNearlyEqual(Volumes[I]->GetCleanliness(), 69.0f));
        Stains[I]->AddWipe(1.0f);
        ++GFrameCounter;
        World->GetTimerManager().Tick(1.0f);
        TestTrue(TEXT("Room has reached 70 cleanliness"), FMath::IsNearlyEqual(Volumes[I]->GetCleanliness(), 70.0f));
        TestEqual(TEXT("70 cleanliness completes the quest room"), Tutorial->GetCleanedRoomCount(), I + 1);
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
    TestEqual(TEXT("Five teams advance to presentation"), Tutorial->GetCurrentStep(), ETutorialStep::AnomalyPresentation);
    const double Previous = Clock->GetTotalGameSeconds();
    Clock->StartClock(86400.0, 60.0); Clock->OnTimeAdvanced.Broadcast(Previous, 86400.0);
    Phase->RequestSleepSettlement(); Phase->ConfirmSettlement();
    TestFalse(TEXT("Day two tutorial inactive"), Tutorial->IsTutorialActive());
    auto* ExtraRoom = World->SpawnActor<ARoomActor>();
    Economy->AddIncome(100.0f);
    TestTrue(TEXT("Paid unlock remains available on day two"), ExtraRoom->TryPurchaseUnlock());
    TestEqual(TEXT("Later day purchase doesn't reactivate tutorial"), Tutorial->GetCurrentStep(), ETutorialStep::NotStarted);
    World->DestroyWorld(false);
    return true;
}
#endif
