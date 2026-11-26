#include "Memo/MemoPaperActor.h"
#include "Memo/MemoSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Memo/MemoBoardActor.h"

AMemoPaperActor::AMemoPaperActor()
{
    PrimaryActorTick.bCanEverTick = false;

    PaperMesh =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaperMesh"));
    SetRootComponent(PaperMesh);
    
    // 선택한 메모를 런타임에 이동·확대하기 위해 필요합니다.
    PaperMesh->SetMobility(EComponentMobility::Movable);

    // 플레이어 이동을 막지 않고 상호작용 시선 검사에만 반응합니다.
    PaperMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PaperMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    PaperMesh->SetCollisionResponseToChannel(
        ECC_Visibility, ECR_Block);

    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

void AMemoPaperActor::BeginPlay()
{
    Super::BeginPlay();

    MemoSystem = GetWorld()->GetSubsystem<UMemoSubsystem>();

    if (!MemoSystem
        || !MemoSystem->ConfigureMemoTable(Memo.DataTable))
    {
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        return;
    }

    MemoSystem->OnMemoStateChanged.AddUniqueDynamic(
        this, &ThisClass::HandleMemoStateChanged);

    // 메모가 이미 해금된 뒤 액터가 로드된 경우도 반영합니다.
    RefreshMemoVisibility();
}

void AMemoPaperActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (MemoSystem)
    {
        MemoSystem->OnMemoStateChanged.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

bool AMemoPaperActor::TryGetReadableMemo(
    FMemoDefinition& OutData) const
{
    return MemoSystem
        && !Memo.RowName.IsNone()
        && MemoSystem->GetMemoState(Memo.RowName)
            != EMemoState::Locked
        && MemoSystem->GetMemoData(Memo.RowName, OutData);
}

bool AMemoPaperActor::CanInteract_Implementation(
    AActor* Interactor) const
{
    FMemoDefinition Data;

    if (!TryGetReadableMemo(Data))
    {
        return false;
    }

    if (IsValid(MemoBoard.Get()))
    {
        return IInteractable::Execute_CanInteract(
            MemoBoard.Get(), Interactor);
    }

    return true;
}

FText AMemoPaperActor::GetInteractPrompt_Implementation(
    AActor* Interactor) const
{
    if (IsValid(MemoBoard.Get()))
    {
        return IInteractable::Execute_GetInteractPrompt(
            MemoBoard.Get(), Interactor);
    }

    return NSLOCTEXT("Memo", "ReadPrompt", "메모 읽기");
}

void AMemoPaperActor::Interact_Implementation(
    AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return;
    }

    // 보드에 붙은 메모를 바라보고 E를 눌러도
    // 먼저 보드 전체를 보는 화면으로 들어갑니다.
    if (IsValid(MemoBoard.Get()))
    {
        IInteractable::Execute_Interact(
            MemoBoard.Get(), Interactor);
        return;
    }

    // 보드에 연결하지 않은 메모는 기존 방식을 유지합니다.
    FMemoDefinition Data;

    if (TryGetReadableMemo(Data))
    {
        ShowMemo(Data.Title, Data.Body);
    }
}

void AMemoPaperActor::ConfirmRead()
{
    if (MemoSystem)
    {
        MemoSystem->MarkMemoRead(Memo.RowName);
    }
}

void AMemoPaperActor::HandleMemoStateChanged(
    FName ChangedMemoId,
    EMemoState NewState)
{
    if (ChangedMemoId == Memo.RowName)
    {
        RefreshMemoVisibility();
    }
}

void AMemoPaperActor::RefreshMemoVisibility()
{
    FMemoDefinition Data;

    const bool bVisible =
        MemoSystem
        && MemoSystem->GetMemoData(Memo.RowName, Data)
        && MemoSystem->GetMemoState(Memo.RowName)
            != EMemoState::Locked;

    SetActorHiddenInGame(!bVisible);
    SetActorEnableCollision(bVisible);
}

