#include "Memo/MemoBoardActor.h"

#include "Memo/MemoPaperActor.h"
#include "Memo/MemoType.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AMemoBoardActor::AMemoBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    USceneComponent* SceneRoot =
        CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);

    BoardMesh =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));

    BoardMesh->SetupAttachment(SceneRoot);
    BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoardMesh->SetCollisionResponseToChannel(
        ECC_Visibility, ECR_Block);

    BoardCamera =
        CreateDefaultSubobject<UCameraComponent>(TEXT("BoardCamera"));

    BoardCamera->SetupAttachment(SceneRoot);
    BoardCamera->bUsePawnControlRotation = false;

    MemoFocusPoint =
        CreateDefaultSubobject<USceneComponent>(TEXT("MemoFocusPoint"));

    MemoFocusPoint->SetupAttachment(BoardCamera);

    // 카메라의 앞쪽은 로컬 +X 방향입니다.
    // 정확한 위치와 회전은 BP에서 메모 크기에 맞게 조절합니다.
    MemoFocusPoint->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
}

bool AMemoBoardActor::CanInteract_Implementation(
    AActor* Interactor) const
{
    const APawn* Pawn = Cast<APawn>(Interactor);

    const APlayerController* PC =
        Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

    if (!PC || !PC->IsLocalController())
    {
        return false;
    }

    if (bBoardOpen)
    {
        return ViewingPC.Get() == PC;
    }

    return BoardWidgetClass != nullptr;
}

FText AMemoBoardActor::GetInteractPrompt_Implementation(
    AActor* Interactor) const
{
    return bBoardOpen
        ? NSLOCTEXT("MemoBoard", "Close", "메모보드 닫기")
        : NSLOCTEXT("MemoBoard", "Open", "메모보드 보기");
}

void AMemoBoardActor::Interact_Implementation(
    AActor* Interactor)
{
    if (!CanInteract_Implementation(Interactor))
    {
        return;
    }

    if (bBoardOpen)
    {
        Back();
        return;
    }

    APawn* Pawn = Cast<APawn>(Interactor);

    APlayerController* PC =
        Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

    OpenBoard(PC);
}

bool AMemoBoardActor::IsInteractionActive_Implementation(
    AActor* Interactor) const
{
    const APawn* Pawn = Cast<APawn>(Interactor);

    return bBoardOpen
        && Pawn
        && Pawn->GetController() == ViewingPC.Get();
}

void AMemoBoardActor::OpenBoard(APlayerController* PC)
{
    if (!PC || bBoardOpen || !BoardWidgetClass)
    {
        return;
    }

    BoardWidget = CreateWidget<UUserWidget>(PC, BoardWidgetClass);

    if (!BoardWidget)
    {
        return;
    }

    ViewingPC = PC;
    PreviousViewTarget = PC->GetViewTarget();
    bPreviousMouseCursor = PC->bShowMouseCursor;

    bBoardOpen = true;
    CameraWaitRemaining = FMath::Max(0.0f, CameraBlendSeconds);

    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);

    PC->SetViewTargetWithBlend(
        this,
        CameraWaitRemaining,
        VTBlend_Cubic,
        0.0f,
        true);

    BoardWidget->SetIsFocusable(true);
    BoardWidget->AddToViewport(100);

    // BP에서 위젯의 BoardReference 등을 설정합니다.
    SetupBoardWidget();

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(BoardWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock);

    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;

    SetActorTickEnabled(true);
}

void AMemoBoardActor::SelectMemoUnderCursor()
{
    APlayerController* PC = ViewingPC.Get();

    if (!bBoardOpen
        || !PC
        || CameraWaitRemaining > 0.0f
        || bHasSelection)
    {
        return;
    }

    FHitResult Hit;

    const bool bHit = PC->GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        Hit);

    if (!bHit)
    {
        return;
    }

    AMemoPaperActor* Paper = Cast<AMemoPaperActor>(Hit.GetActor());

    // 다른 보드의 메모나 장식용 메시를 선택하지 않습니다.
    if (!IsValid(Paper) || Paper->MemoBoard.Get() != this)
    {
        return;
    }

    FMemoDefinition Data;

    if (!Paper->TryGetReadableMemo(Data))
    {
        return;
    }

    SelectedMemo = Paper;
    OriginalMemoTransform = Paper->GetActorTransform();

    FocusMemoTransform = FTransform(
        MemoFocusPoint->GetComponentQuat(),
        MemoFocusPoint->GetComponentLocation(),
        OriginalMemoTransform.GetScale3D()
            * FocusScaleMultiplier);

    bHasSelection = true;
    bMemoArrived = false;
    MemoMoveElapsed = 0.0f;
}

void AMemoBoardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bBoardOpen)
    {
        return;
    }

    if (!ViewingPC.IsValid())
    {
        CloseBoard();
        return;
    }

    CameraWaitRemaining = FMath::Max(
        0.0f, CameraWaitRemaining - DeltaSeconds);

    if (!bHasSelection)
    {
        return;
    }

    AMemoPaperActor* Paper = SelectedMemo.Get();

    // 메모가 포함된 레벨이 내려가거나 액터가 제거된 경우입니다.
    if (!IsValid(Paper))
    {
        Back();
        return;
    }

    if (bMemoArrived)
    {
        return;
    }

    MemoMoveElapsed += DeltaSeconds;

    const float Alpha = FMath::Clamp(
        MemoMoveElapsed / FMath::Max(0.01f, MemoMoveSeconds),
        0.0f,
        1.0f);

    // 시작과 끝을 부드럽게 합니다.
    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

    FTransform CurrentTransform;
    CurrentTransform.Blend(
        OriginalMemoTransform,
        FocusMemoTransform,
        SmoothAlpha);

    Paper->SetActorTransform(
        CurrentTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    if (Alpha >= 1.0f)
    {
        bMemoArrived = true;

        FMemoDefinition Data;

        if (Paper->TryGetReadableMemo(Data))
        {
            ShowMemoDetails(Data.Title, Data.Body);
        }
        else
        {
            Back();
        }
    }
}

void AMemoBoardActor::ConfirmFocusedMemoRead()
{
    if (!bBoardOpen || !bHasSelection || !bMemoArrived)
    {
        return;
    }

    if (AMemoPaperActor* Paper = SelectedMemo.Get())
    {
        Paper->ConfirmRead();
    }
}

void AMemoBoardActor::RestoreSelectedMemo()
{
    if (AMemoPaperActor* Paper = SelectedMemo.Get())
    {
        Paper->SetActorTransform(
            OriginalMemoTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }

    SelectedMemo.Reset();
    bHasSelection = false;
    bMemoArrived = false;
    MemoMoveElapsed = 0.0f;
}

void AMemoBoardActor::Back()
{
    if (!bBoardOpen)
    {
        return;
    }

    if (bHasSelection)
    {
        RestoreSelectedMemo();
        HideMemoDetails();

        if (BoardWidget)
        {
            BoardWidget->SetKeyboardFocus();
        }

        return;
    }

    CloseBoard();
}

void AMemoBoardActor::CloseBoard()
{
    if (!bBoardOpen)
    {
        return;
    }

    bBoardOpen = false;

    RestoreSelectedMemo();

    if (BoardWidget)
    {
        BoardWidget->RemoveFromParent();
        BoardWidget = nullptr;
    }

    if (APlayerController* PC = ViewingPC.Get())
    {
        AActor* ReturnTarget = PreviousViewTarget.Get();

        if (!ReturnTarget)
        {
            ReturnTarget = PC->GetPawn();
        }

        if (ReturnTarget)
        {
            PC->SetViewTargetWithBlend(ReturnTarget, 0.0f);
        }

        // 이 보드를 열면서 추가한 입력 잠금을 해제합니다.
        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);

        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = bPreviousMouseCursor;
    }

    ViewingPC.Reset();
    PreviousViewTarget.Reset();
    CameraWaitRemaining = 0.0f;

    SetActorTickEnabled(false);
}

void AMemoBoardActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    CloseBoard();

    Super::EndPlay(EndPlayReason);
}

