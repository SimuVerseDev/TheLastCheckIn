#include "FoodService/Cooking/CookingStation.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "Engine/World.h"
#include "FoodService/Data/FoodDefinition.h"
#include "FoodService/Items/FoodItemActor.h"
#include "FoodService/Placement/FoodPlacement.h"
#include "FoodService/Placement/FoodSnapSlotComponent.h"

ACookingStation::ACookingStation()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
    StationMesh->SetupAttachment(SceneRoot);
    StationMesh->SetCollisionProfileName(TEXT("BlockAll"));
    StationMesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);

    CookingSlot = CreateDefaultSubobject<UFoodSnapSlotComponent>(TEXT("CookingSlot"));
    CookingSlot->SetupAttachment(SceneRoot);
    CookingSlot->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
    CookingSlot->Rule = EFoodSlotRule::RawOnly;
    CookingSlot->bAllowPickup = true;
}

void ACookingStation::BeginPlay()
{
    Super::BeginPlay();

    if (UGameClockSubsystem* Clock =
        GetWorld()->GetSubsystem<UGameClockSubsystem>())
    {
        Clock->OnTimeAdvanced.AddUniqueDynamic(this, &ACookingStation::HandleTimeAdvanced);
    }
}

void ACookingStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>())
        {
            Clock->OnTimeAdvanced.RemoveDynamic(this, &ACookingStation::HandleTimeAdvanced);
        }
    }

    CancelCurrentCooking();

    Super::EndPlay(EndPlayReason);
}

/*
Interact : E키
*/
bool ACookingStation::CanInteract_Implementation(AActor* Interactor) const
{
    if (!GetWorld()
        || !GetWorld()->GetSubsystem<UGameClockSubsystem>()
        || !IsValid(CookingSlot.Get()))
    {
        return false;
    }

    return CookingSlot->CanAccept(
        FoodPlacement::GetHeldFood(Interactor));
}

bool ACookingStation::CanInteractWhileCarrying_Implementation(AActor* Interactor) const
{
    return CanInteract_Implementation(Interactor);
}

FText ACookingStation::GetInteractPrompt_Implementation(AActor* Interactor) const
{
    return NSLOCTEXT("Food", "StartCooking", "음식 놓고 조리하기");
}

void ACookingStation::Interact_Implementation(AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return;
    }

    UGameClockSubsystem* Clock =GetWorld()->GetSubsystem<UGameClockSubsystem>();

    if (!Clock
        || !FoodPlacement::TryPlaceHeldFood(
            Interactor,
            CookingSlot.Get()))
    {
        return;
    }

    AFoodItemActor* Food = CookingSlot->GetFood();

    if (!IsValid(Food) || !Food->BeginCooking())
    {
        return;
    }

    CookingFood = Food;
    CookingLease = Food->GetLeaseId();

    const double CookingSeconds = GameTime::MinutesToSeconds(
        FMath::Max(0.1f, Food->GetFoodDefinition()->CookingGameMinutes));

    FinishAtGameSeconds = Clock->GetTotalGameSeconds() + CookingSeconds;
}

/*
조리 진행
*/
void ACookingStation::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
    AFoodItemActor* Food = CookingFood.Get();

    if (!IsValid(Food)
        || !Food->MatchesLease(CookingLease)
        || Food->GetCurrentSlot() != CookingSlot.Get()
        || Food->GetCookState() != EFoodCookState::Cooking)
    {
        CancelCurrentCooking();
        return;
    }

    if (CurrentSeconds < FinishAtGameSeconds)
    {
        return;
    }

    Food->FinishCooking();

    CookingFood.Reset();
    CookingLease.Invalidate();
    FinishAtGameSeconds = -1.0;
}

void ACookingStation::CancelCurrentCooking()
{
    AFoodItemActor* Food = CookingFood.Get();

    if (IsValid(Food) && Food->MatchesLease(CookingLease))
    {
        Food->CancelCooking();
    }

    CookingFood.Reset();
    CookingLease.Invalidate();
    FinishAtGameSeconds = -1.0;
}