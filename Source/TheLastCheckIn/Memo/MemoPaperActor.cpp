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

    // 정상 메모는 처음부터 보이도록 설정
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
}

void AMemoPaperActor::BeginPlay()
{
    Super::BeginPlay();

    MemoSystem = GetWorld()->GetSubsystem<UMemoSubsystem>();

    if (!MemoSystem
    || !MemoSystem->ConfigureMemoTable(Memo.DataTable))
    {
        // 데이터 설정이 잘못된 메모는 숨깁니다.
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
    // 보드에 붙은 종이는 잠겨 있어도 보드 보기로 연결합니다.
    if (IsValid(MemoBoard.Get()))
    {
        return IInteractable::Execute_CanInteract(
            MemoBoard.Get(), Interactor);
    }

    // 보드에 연결되지 않은 단독 메모는 해금됐을 때만 읽습니다.
    FMemoDefinition Data;
    return TryGetReadableMemo(Data);
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

    // 정상적인 메모 데이터가 있으면 잠금 여부와 관계없이 표시합니다.
    const bool bHasValidMemo =
        MemoSystem
        && MemoSystem->GetMemoData(Memo.RowName, Data);

    SetActorHiddenInGame(!bHasValidMemo);
    SetActorEnableCollision(bHasValidMemo);

    // 해금됐지만 아직 읽지 않은 메모만 강조합니다.
    const bool bHighlight =
        bHasValidMemo
        && MemoSystem->GetMemoState(Memo.RowName)
            == EMemoState::Unread;

    if (PaperMesh)
    {
        // 이전에 적용한 종이 전체 발광은 끕니다.
        PaperMesh->SetScalarParameterValueOnMaterials(
            FName(TEXT("MemoHighlight")), 0.0f);

        // 7은 메모 하이라이트용 식별 번호입니다.
        PaperMesh->SetCustomDepthStencilValue(7);

        // 해금됐지만 아직 읽지 않은 동안만 활성화합니다.
        PaperMesh->SetRenderCustomDepth(bHighlight);
    }
}

