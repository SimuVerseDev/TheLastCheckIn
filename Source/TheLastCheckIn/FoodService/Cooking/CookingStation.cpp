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

bool ACookingStation::CanUse_Implementation(AActor* Interactor) const
{
    if (!IsValid(Interactor) || !IsValid(CookingSlot.Get()))
    {
        return false;
    }

    AFoodItemActor* Food = CookingSlot->GetFood();

    // 조리가 끝나고 집을 수 있는 음식만 꺼낸다.
    return IsValid(Food)
        && Food->GetCookState() == EFoodCookState::Cooked
        && IInteractable::Execute_CanUse(Food, Interactor);
}

FText ACookingStation::GetUsePrompt_Implementation(AActor* Interactor) const
{
    return NSLOCTEXT(
        "Food",
        "TakeCookedFood",
        "완성된 음식 꺼내기");
}

bool ACookingStation::Use_Implementation(AActor* Interactor)
{
    if (!CanUse_Implementation(Interactor))
    {
        return false;
    }

    AFoodItemActor* Food = CookingSlot->GetFood();

    // 기존 음식 집기에서 물리 핸들 연결과 슬롯 비우기를 처리한다.
    return IsValid(Food)
        && IInteractable::Execute_Use(Food, Interactor);
}