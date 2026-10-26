#include "CheckInInteractionComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Guests/GuestCharacter.h"
#include "Rooms/RoomActor.h"
#include "Housekeeping/Carry/HKCarryComponent.h"

// Sets default values for this component's properties
UCheckInInteractionComponent::UCheckInInteractionComponent()
{

	PrimaryComponentTick.bCanEverTick = false;
	
	static ConstructorHelpers::FObjectFinder<UInputAction>
		InteractFinder(
			TEXT("/Game/Input/Actions/IA_Interact.IA_Interact"));

	if (InteractFinder.Succeeded())
	{
		InteractAction = InteractFinder.Object;
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
    bWaitForKeyRelease = true;

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
    SetInputRegistered(false);

    bInsideCounter = false;
    bWaitForKeyRelease = true;

    CounterTarget.Reset();
    CurrentGuest.Reset();
}

void UCheckInInteractionComponent::CreateCheckInInput()
{
    if (CheckInInput || !InteractAction)
    {
        return;
    }

    CheckInInput = NewObject<UEnhancedInputComponent>(
        GetOwner(), TEXT("CheckInInput"));

    CheckInInput->RegisterComponent();

    // 현재 HK 입력보다 높은 우선순위
    CheckInInput->Priority = 100;

    CheckInInput->bBlockInput = false;

    auto& Binding = CheckInInput->BindAction(
        InteractAction,
        ETriggerEvent::Started,
        this,
        &UCheckInInteractionComponent::HandleCheckInPressed);
    
    Binding.SetShouldConsume(true);
}

void UCheckInInteractionComponent::SetInputRegistered(bool bRegister)
{
    if (!IsValid(LocalController) || !CheckInInput)
    {
        bInputRegistered = false;
        return;
    }

    if (bRegister == bInputRegistered)
    {
        return;
    }

    if (bRegister)
    {
        LocalController->PushInputComponent(CheckInInput);
    }
    else
    {
        LocalController->PopInputComponent(CheckInInput);
    }

    bInputRegistered = bRegister;
}

bool UCheckInInteractionComponent::IsInteractionKeyDown() const
{
    if (!IsValid(LocalController) || !InteractAction)
    {
        return false;
    }

    const ULocalPlayer* LocalPlayer =
        LocalController->GetLocalPlayer();

    const UEnhancedInputLocalPlayerSubsystem* Subsystem =
        LocalPlayer
        ? LocalPlayer->GetSubsystem<
            UEnhancedInputLocalPlayerSubsystem>()
        : nullptr;

    if (!Subsystem)
    {
        return false;
    }

    for (const FKey& Key :
        Subsystem->QueryKeysMappedToAction(InteractAction))
    {
        if (LocalController->IsInputKeyDown(Key))
        {
            return true;
        }
    }

    return false;
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

        if (Guest->GetGuestState()
            != EGuestState::WaitingForCheckIn)
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

    return
        Guest->GetGuestState() == EGuestState::WaitingForCheckIn
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
        SetInputRegistered(false);

        CheckInWidget = nullptr;
        LocalController = NewController;

        bWaitForKeyRelease = true;
    }

    if (!IsValid(LocalController))
    {
        return;
    }

    CreateCheckInInput();

    if (bCheckInOpen)
    {
        if (!bAssignedThisSession && !IsCurrentGuestReady())
        {
            CloseCheckIn();
            return;
        }

        OnRoomListChanged.Broadcast();
        return;
    }


    if (bWaitForKeyRelease)
    {
        if (IsInteractionKeyDown()
            || LocalController->IsInputKeyDown(EKeys::LeftMouseButton))
        {
            return;
        }

        bWaitForKeyRelease = false;
    }

    if (!CanOpenCheckIn())
    {
        SetInputRegistered(false);
        return;
    }

    if (!bInputRegistered)
    {
        if (IsInteractionKeyDown()
            || LocalController->IsInputKeyDown(EKeys::LeftMouseButton))
        {
            return;
        }

        SetInputRegistered(true);
    }
}

void UCheckInInteractionComponent::HandleCheckInPressed()
{
    if (bWaitForKeyRelease || !CanOpenCheckIn())
    {
        return;
    }

    AGuestCharacter* Guest = FindWaitingGuest();

    if (!IsValid(Guest))
    {
        return;
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

        return;
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

    bWaitForKeyRelease = true;
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

    bSelectable =
        bInsideCounter
        && bCheckInOpen
        && !bAssignedThisSession
        && IsCurrentGuestReady()
        && State == ERoomState::Available
        && IsValid(Room->GetArrivalTarget())
        && Room->GetCapacity() > 0;
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

    const bool bSuccess = Guest->TryAssignRoom(Room);

    if (bSuccess)
    {
        bAssignedThisSession = true;
        CurrentGuest.Reset();
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
    EndCounterInteraction();

    if (IsValid(CheckInInput))
    {
        CheckInInput->DestroyComponent();
        CheckInInput = nullptr;
    }

    CheckInWidget = nullptr;
    LocalController = nullptr;

    Super::EndPlay(EndPlayReason);
}