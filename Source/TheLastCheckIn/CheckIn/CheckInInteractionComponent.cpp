#include "CheckInInteractionComponent.h"
#include "CheckInCounter.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Guests/GuestCharacter.h"
#include "Guests/Party/GuestParty.h"
#include "Rooms/RoomActor.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"

// Sets default values for this component's properties
UCheckInInteractionComponent::UCheckInInteractionComponent()
{

	PrimaryComponentTick.bCanEverTick = false;
}

void UCheckInInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Blueprint components and their initial overlaps must finish initializing first.
    InitialCounterCheckTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
        this, &UCheckInInteractionComponent::CheckInitialCounterOverlap);
}

void UCheckInInteractionComponent::CheckInitialCounterOverlap()
{
    AActor* Owner = GetOwner();
    if (bInsideCounter || !IsValid(Owner))
    {
        return;
    }

    TArray<AActor*> OverlappingActors;
    Owner->GetOverlappingActors(OverlappingActors);

    for (AActor* Counter : OverlappingActors)
    {
        if (!IsValid(Counter))
        {
            continue;
        }

        // BP_CheckInCounter currently owns these members in Blueprint, not C++.
        // Only its player interaction volume should enable check-in.
        TInlineComponentArray<UPrimitiveComponent*> Primitives(Counter);
        for (UPrimitiveComponent* Primitive : Primitives)
        {
            if (Primitive->GetFName() != TEXT("PlayerInteractionBox")
                || !Primitive->IsOverlappingActor(Owner))
            {
                continue;
            }

            const FObjectPropertyBase* TargetProperty =
                FindFProperty<FObjectPropertyBase>(Counter->GetClass(), TEXT("CounterTarget"));
            AActor* Target = TargetProperty
                ? Cast<AActor>(TargetProperty->GetObjectPropertyValue_InContainer(Counter))
                : nullptr;

            if (IsValid(Target))
            {
                BeginCounterInteraction(Target);
                return;
            }
        }
    }
}

void UCheckInInteractionComponent::BeginCounterInteraction(
    AActor* InCounterTarget)
{
    if (!IsValid(InCounterTarget) || !GetWorld())
    {
        return;
    }

    if (bInsideCounter && CounterTarget.Get() == InCounterTarget)
    {
        return;
    }

    EndCounterInteraction();

    CounterTarget = InCounterTarget;
    bInsideCounter = true;

    GetWorld()->GetTimerManager().SetTimer(
        CounterCheckTimer,
        this,
        &UCheckInInteractionComponent::CheckCounterState,
        FMath::Max(CheckInterval, 0.05f),
        true);


    CheckCounterState();
}

void UCheckInInteractionComponent::EndCounterInteraction()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(
            CounterCheckTimer);
    }

    CloseCheckIn();

    bInsideCounter = false;

    CounterTarget.Reset();
    CurrentGuest.Reset();
}

AActor* UCheckInInteractionComponent::FindInteractionTarget(const FVector& Start, const FVector& End) const
{
    const AActor* Owner = GetOwner();
    if (!bInsideCounter || !IsValid(Owner) || !GetWorld())
    {
        return nullptr;
    }

    for (TActorIterator<ACheckInCounter> It(GetWorld()); It; ++It)
    {
        if (Owner->IsOverlappingActor(*It))
        {
            return *It;
        }
    }
    return nullptr;
}

bool UCheckInInteractionComponent::IsCarryingObject() const
{

    const UHKCarryComponent* Carry =
        GetOwner()->FindComponentByClass<UHKCarryComponent>();

    return Carry && Carry->IsCarrying();
}

AGuestCharacter*UCheckInInteractionComponent::FindWaitingGuest() const
{
    const AActor* Target = CounterTarget.Get();

    if (!IsValid(Target) || !GetWorld())
    {
        return nullptr;
    }

    AGuestCharacter* BestGuest = nullptr;

    double BestDistanceSquared =
        FMath::Square(static_cast<double>(GuestRange));

    for (TActorIterator<AGuestCharacter> It(GetWorld()); It; ++It)
    {
        AGuestCharacter* Guest = *It;

        if (!IsValid(Guest) || Guest->IsHidden())
        {
            continue;
        }

        UGuestParty* Party = Guest->GetParty();

        if (!IsValid(Party) ||
            Party->GetLeader() != Guest ||
            !Party->CanCheckIn())
        {
            continue;
        }
        
        if (Guest->GetInitialMoveTarget() != Target)
        {
            continue;
        }

        const double DistanceSquared = FVector::DistSquared(
            Guest->GetActorLocation(),
            Target->GetActorLocation());

        if (DistanceSquared <= BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestGuest = Guest;
        }
    }

    return BestGuest;
}

bool UCheckInInteractionComponent::IsCurrentGuestReady() const
{
    const AGuestCharacter* Guest = CurrentGuest.Get();
    const AActor* Target = CounterTarget.Get();

    if (!IsValid(Guest) || !IsValid(Target) || Guest->IsHidden())
    {
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    return
        IsValid(Party)
        && Party->GetLeader() == Guest
        && Party->CanCheckIn()
        && Guest->GetInitialMoveTarget() == Target
        && FVector::DistSquared(
            Guest->GetActorLocation(),
            Target->GetActorLocation()) <= FMath::Square(GuestRange);
}

bool UCheckInInteractionComponent::CanOpenCheckIn() const
{
    return
        bInsideCounter
        && !bCheckInOpen
        && CounterTarget.IsValid()
        && IsValid(LocalController)
        && CheckInWidgetClass.Get() != nullptr
        && !IsCarryingObject()
        && !LocalController->IsInputKeyDown(EKeys::LeftMouseButton)
        && FindWaitingGuest() != nullptr;
}

void UCheckInInteractionComponent::CheckCounterState()
{
    if (!bInsideCounter || !CounterTarget.IsValid())
    {
        EndCounterInteraction();
        return;
    }

    APawn* Pawn = Cast<APawn>(GetOwner());

    APlayerController* NewController =
        Pawn
        ? Cast<APlayerController>(Pawn->GetController())
        : nullptr;

    if (NewController && !NewController->IsLocalController())
    {
        NewController = nullptr;
    }
    
    if (LocalController.Get() != NewController)
    {
        CloseCheckIn();

        CheckInWidget = nullptr;
        LocalController = NewController;
    }

    if (!IsValid(LocalController))
    {
        return;
    }

    if (bCheckInOpen)
    {
        if (!bAssignedThisSession && !IsCurrentGuestReady())
        {
            CloseCheckIn();
            return;
        }

        OnRoomListChanged.Broadcast();
    }
}

bool UCheckInInteractionComponent::OpenCheckIn()
{
    if (!CanOpenCheckIn())
    {
        return false;
    }

    AGuestCharacter* Guest = FindWaitingGuest();

    if (!IsValid(Guest))
    {
        return false;
    }

    if (!IsValid(CheckInWidget))
    {
        CheckInWidget = CreateWidget<UUserWidget>(
            LocalController.Get(),
            CheckInWidgetClass);
    }

    if (!IsValid(CheckInWidget))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("CheckIn: Failed to create the sidebar widget."));

        return false;
    }

    CurrentGuest = Guest;
    bAssignedThisSession = false;
    bCheckInOpen = true;

    bPreviousMouseCursor =
        LocalController->bShowMouseCursor;

    CheckInWidget->AddToViewport(20);

    LocalController->bShowMouseCursor = true;
    LocalController->SetIgnoreMoveInput(true);
    LocalController->SetIgnoreLookInput(true);

    UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(
        LocalController.Get(),
        CheckInWidget.Get(),
        EMouseLockMode::DoNotLock,
        true);

    CheckInWidget->SetUserFocus(LocalController.Get());

    OnRoomListChanged.Broadcast();
    return true;
}

void UCheckInInteractionComponent::CloseCheckIn()
{
    if (!bCheckInOpen)
    {
        return;
    }

    bCheckInOpen = false;
    bAssignedThisSession = false;

    CurrentGuest.Reset();

    if (IsValid(CheckInWidget))
    {
        CheckInWidget->RemoveFromParent();
    }

    if (IsValid(LocalController))
    {
        LocalController->SetIgnoreMoveInput(false);
        LocalController->SetIgnoreLookInput(false);

        LocalController->bShowMouseCursor =
            bPreviousMouseCursor;

        UWidgetBlueprintLibrary::SetInputMode_GameOnly(
            LocalController.Get(),
            true);
    }
}

ARoomActor* UCheckInInteractionComponent::FindUniqueRoom(int32 RoomNumber) const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    ARoomActor* FoundRoom = nullptr;

    for (TActorIterator<ARoomActor> It(GetWorld()); It; ++It)
    {
        ARoomActor* Room = *It;

        if (!IsValid(Room)
            || Room->GetRoomNumber() != RoomNumber)
        {
            continue;
        }
        
        if (FoundRoom)
        {
            return nullptr;
        }

        FoundRoom = Room;
    }

    return FoundRoom;
}

void UCheckInInteractionComponent::GetRoomStatus(
    int32 RoomNumber,
    bool& bRoomExists,
    bool& bAssigned,
    bool& bSelectable) const
{
    bRoomExists = false;
    bAssigned = false;
    bSelectable = false;

    const ARoomActor* Room = FindUniqueRoom(RoomNumber);

    if (!IsValid(Room))
    {
        return;
    }

    bRoomExists = true;

    const ERoomState State = Room->GetRoomState();

    bAssigned =
        State == ERoomState::Assigned
        || State == ERoomState::Occupied;

    UGuestParty* Party = CurrentGuest.IsValid()
        ? CurrentGuest.Get()->GetParty()
        : nullptr;

    bSelectable =
        bInsideCounter
        && bCheckInOpen
        && !bAssignedThisSession
        && IsCurrentGuestReady()
        && IsValid(Party)
        && Party->CanAssignRoom(Room);
}

bool UCheckInInteractionComponent::TryAssignRoomNumber(int32 RoomNumber)
{
    bool bRoomExists;
    bool bAssigned;
    bool bSelectable;

    GetRoomStatus(RoomNumber,bRoomExists,bAssigned,bSelectable);

    if (!bSelectable)
    {
        OnRoomListChanged.Broadcast();
        return false;
    }

    AGuestCharacter* Guest = CurrentGuest.Get();
    ARoomActor* Room = FindUniqueRoom(RoomNumber);

    if (!IsValid(Guest) || !IsValid(Room))
    {
        OnRoomListChanged.Broadcast();
        return false;
    }

    UGuestParty* Party = Guest->GetParty();

    const bool bSuccess =
        IsValid(Party)
        && Party->GetLeader() == Guest
        && Party->TryAssignRoom(Room);

    if (bSuccess)
    {
        CloseCheckIn();
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("CheckIn: Failed to assign room %d."),
            RoomNumber);
    }

    OnRoomListChanged.Broadcast();

    return bSuccess;
}

void UCheckInInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(InitialCounterCheckTimer);
    }

    EndCounterInteraction();

    CheckInWidget = nullptr;
    LocalController = nullptr;

    Super::EndPlay(EndPlayReason);
}
