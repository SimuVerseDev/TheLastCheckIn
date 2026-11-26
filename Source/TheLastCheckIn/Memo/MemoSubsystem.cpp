#include "Memo/MemoSubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"

void UMemoSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
    CurrentDay = Phase->GetCurrentDay();

    Phase->OnDayStarted.AddDynamic(
        this, &ThisClass::HandleDayStarted);
}

void UMemoSubsystem::Deinitialize()
{
    if (Phase)
    {
        Phase->OnDayStarted.RemoveAll(this);
    }

    OnMemoStateChanged.Clear();
    MemoStates.Reset();
    ReportedEvents.Reset();
    MemoTable = nullptr;

    Super::Deinitialize();
}

bool UMemoSubsystem::ConfigureMemoTable(const UDataTable* InTable)
{
    if (!IsValid(InTable)
        || InTable->GetRowStruct() != FMemoDefinition::StaticStruct())
    {
        return false;
    }

    // 진행 중에 다른 테이블로 바꾸어 ID가 섞이지 않도록 합니다.
    if (MemoTable && MemoTable != InTable)
    {
        UE_LOG(
            LogTemp, Warning,
            TEXT("[Memo] All memo papers must use the same table."));
        return false;
    }

    MemoTable = InTable;
    EvaluateUnlocks();
    return true;
}

EMemoState UMemoSubsystem::GetMemoState(FName MemoId) const
{
    const EMemoState* State = MemoStates.Find(MemoId);
    return State ? *State : EMemoState::Locked;
}

bool UMemoSubsystem::IsMemoRead(FName MemoId) const
{
    return GetMemoState(MemoId) == EMemoState::Read;
}

bool UMemoSubsystem::GetMemoData(
    FName MemoId,
    FMemoDefinition& OutData) const
{
    OutData = FMemoDefinition();

    if (!MemoTable || MemoId.IsNone())
    {
        return false;
    }

    const FMemoDefinition* Row =
        MemoTable->FindRow<FMemoDefinition>(
            MemoId, TEXT("Memo"), false);

    if (!Row)
    {
        return false;
    }

    OutData = *Row;
    return true;
}

void UMemoSubsystem::ReportMemoEvent(FName EventId)
{
    if (EventId.IsNone() || ReportedEvents.Contains(EventId))
    {
        return;
    }

    ReportedEvents.Add(EventId);
    EvaluateUnlocks();
}

bool UMemoSubsystem::MarkMemoRead(FName MemoId)
{
    // 잠긴 메모나 이미 읽은 메모는 변경하지 않습니다.
    if (GetMemoState(MemoId) != EMemoState::Unread)
    {
        return false;
    }

    MemoStates.Add(MemoId, EMemoState::Read);
    OnMemoStateChanged.Broadcast(MemoId, EMemoState::Read);

    // 이 메모를 선행 조건으로 기다리던 메모를 확인합니다.
    EvaluateUnlocks();
    return true;
}

void UMemoSubsystem::HandleDayStarted(int32 Day)
{
    CurrentDay = Day;
    EvaluateUnlocks();
}

void UMemoSubsystem::EvaluateUnlocks()
{
    if (!MemoTable)
    {
        return;
    }

    TArray<FName> NewlyUnlocked;

    for (const FName MemoId : MemoTable->GetRowNames())
    {
        if (GetMemoState(MemoId) != EMemoState::Locked)
        {
            continue;
        }

        const FMemoDefinition* Row =
            MemoTable->FindRow<FMemoDefinition>(
                MemoId, TEXT("MemoUnlock"), false);

        if (!Row || CurrentDay < Row->UnlockDay)
        {
            continue;
        }

        if (!Row->RequiredEvent.IsNone()
            && !ReportedEvents.Contains(Row->RequiredEvent))
        {
            continue;
        }

        if (!Row->RequiredReadMemo.IsNone()
            && !IsMemoRead(Row->RequiredReadMemo))
        {
            continue;
        }

        MemoStates.Add(MemoId, EMemoState::Unread);
        NewlyUnlocked.Add(MemoId);
    }

    // 상태 갱신을 마친 뒤 액터와 UI에 알립니다.
    for (const FName MemoId : NewlyUnlocked)
    {
        OnMemoStateChanged.Broadcast(
            MemoId, GetMemoState(MemoId));
    }
}
