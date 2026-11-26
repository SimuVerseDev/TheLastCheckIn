#include "FoodService/Pool/FoodPoolSubsystem.h"

#include "FoodService/Items/FoodItemActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

AFoodItemActor* UFoodPoolSubsystem::CreateFoodActor()
{
    UWorld* World = GetWorld();

    if (bShuttingDown || !World || !World->IsGameWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;

    // 개별 구역을 언로드해도 유지되도록 기본 레벨에 생성한다.
    Params.OverrideLevel = World->PersistentLevel;

    AFoodItemActor* Food = World->SpawnActor<AFoodItemActor>(
        AFoodItemActor::StaticClass(),
        FTransform::Identity,
        Params);

    if (IsValid(Food))
    {
        Food->PoolOwner = this;
        Food->ResetForPool();
    }

    return Food;
}

AFoodItemActor* UFoodPoolSubsystem::AcquireFood(UFoodDefinition* Definition, EFoodCookState InitialState, const FTransform& Transform)
{
    if (bShuttingDown
        || !IsValid(Definition)
        || Definition->FoodId.IsNone()
        || InitialState == EFoodCookState::Cooking
        || Transform.ContainsNaN())
    {
        return nullptr;
    }

    UStaticMesh* SelectedMesh =
        (!Definition->bRequiresCooking || InitialState == EFoodCookState::Cooked)
        ? Definition->CookedMesh.Get()
        : Definition->RawMesh.Get();

    if (!IsValid(SelectedMesh))
    {
        return nullptr;
    }

    AFoodItemActor* Food = nullptr;

    while (!AvailableFoods.IsEmpty())
    {
        Food = AvailableFoods.Pop().Get();

        if (IsValid(Food) && !Food->IsActorBeingDestroyed())
        {
            break;
        }

        Food = nullptr;
    }

    if (!Food)
    {
        Food = CreateFoodActor();
    }

    if (IsValid(Food))
    {
        Food->ActivateFood(Definition, InitialState, Transform);
    }

    return Food;
}
bool UFoodPoolSubsystem::ReleaseFood(AFoodItemActor* Food, FGuid ExpectedLeaseId)
{
    // 들고 있는 음식은 물리 핸들이 연결되어 있으므로 풀 반환을 거절한다.
    if (bShuttingDown
        || !IsValid(Food)
        || Food->GetWorld() != GetWorld()
        || Food->PoolOwner.Get() != this
        || !Food->MatchesLease(ExpectedLeaseId)
        || Food->IsCarried())
    {
        return false;
    }

    Food->ResetForPool();

    if (GetAvailableCount() >= MaxRetainedFoods)
    {
        Food->Destroy();
    }
    else
    {
        AvailableFoods.Add(Food);
    }

    return true;
}

void UFoodPoolSubsystem::WarmupPool(int32 DesiredCount)
{
    AvailableFoods.RemoveAll([](const TObjectPtr<AFoodItemActor>& Food)
        {
            return !IsValid(Food.Get()) || Food->IsActorBeingDestroyed();
        });

    const int32 TargetCount =
        FMath::Clamp(DesiredCount, 0, MaxRetainedFoods);

    while (AvailableFoods.Num() < TargetCount)
    {
        AFoodItemActor* Food = CreateFoodActor();

        if (!IsValid(Food))
        {
            break;
        }

        AvailableFoods.Add(Food);
    }
}

int32 UFoodPoolSubsystem::GetAvailableCount() const
{
    int32 Count = 0;

    for (const TObjectPtr<AFoodItemActor>& Food : AvailableFoods)
    {
        if (IsValid(Food.Get()) && !Food->IsActorBeingDestroyed())
        {
            ++Count;
        }
    }

    return Count;
}

void UFoodPoolSubsystem::Deinitialize()
{
    bShuttingDown = true;
    AvailableFoods.Reset();

    // 목록만 정리한다. 음식 액터는 월드 종료 시 함께 제거된다.
    Super::Deinitialize();
}
