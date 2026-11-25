#include "Memo/MemoPaperActor.h"
#include "Memo/MemoSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

AMemoPaperActor::AMemoPaperActor()
{
    PrimaryActorTick.bCanEverTick = false;

    PaperMesh =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaperMesh"));
    SetRootComponent(PaperMesh);

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

bool AMemoPaperActor::CanInteract_Implementation(
    AActor* Interactor) const
{
    if (!MemoSystem || Memo.RowName.IsNone())
    {
        return false;
    }

    FMemoDefinition Data;

    return MemoSystem->GetMemoState(Memo.RowName)
            != EMemoState::Locked
        && MemoSystem->GetMemoData(Memo.RowName, Data);
}

FText AMemoPaperActor::GetInteractPrompt_Implementation(
    AActor* Interactor) const
{
    return NSLOCTEXT("Memo", "ReadPrompt", "메모 읽기");
}

void AMemoPaperActor::Interact_Implementation(
    AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return;
    }

    FMemoDefinition Data;

    if (MemoSystem->GetMemoData(Memo.RowName, Data))
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

