#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "MemoBoardActor.generated.h"

class UStaticMeshComponent;
class UCameraComponent;
class USceneComponent;
class UUserWidget;
class APlayerController;
class AMemoPaperActor;

UCLASS()
class THELASTCHECKIN_API AMemoBoardActor
    : public AActor
    , public IInteractable
{
    GENERATED_BODY()

public:
    AMemoBoardActor();

    virtual void Tick(float DeltaSeconds) override;

    virtual bool CanInteract_Implementation(
        AActor* Interactor) const override;

    virtual FText GetInteractPrompt_Implementation(
        AActor* Interactor) const override;

    virtual void Interact_Implementation(
        AActor* Interactor) override;

    virtual bool IsInteractionActive_Implementation(
        AActor* Interactor) const override;

    // 보드 UI의 배경 클릭에서 호출합니다.
    UFUNCTION(BlueprintCallable, Category = "Memo|Board")
    void SelectMemoUnderCursor();

    // 메모를 보고 있으면 메모를 닫고,
    // 보드만 보고 있으면 보드를 닫습니다.
    UFUNCTION(BlueprintCallable, Category = "Memo|Board")
    void Back();

    UFUNCTION(BlueprintCallable, Category = "Memo|Board")
    void CloseBoard();

    // 제목·내용 UI를 실제로 표시한 후 호출합니다.
    UFUNCTION(BlueprintCallable, Category = "Memo|Board")
    void ConfirmFocusedMemoRead();

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Memo|Board")
    TObjectPtr<UStaticMeshComponent> BoardMesh;

    // 보드 전체를 바라보는 고정 카메라입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Memo|Board")
    TObjectPtr<UCameraComponent> BoardCamera;

    // 선택한 메모가 이동할 위치·회전입니다.
    // 카메라의 자식이므로 카메라 앞에 배치하면 됩니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Memo|Board")
    TObjectPtr<USceneComponent> MemoFocusPoint;

    UPROPERTY(EditDefaultsOnly, Category = "Memo|Board")
    TSubclassOf<UUserWidget> BoardWidgetClass;

    UPROPERTY(EditAnywhere, Category = "Memo|Board",
        meta = (ClampMin = "0.0"))
    float CameraBlendSeconds = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Memo|Board",
        meta = (ClampMin = "0.01"))
    float MemoMoveSeconds = 0.3f;

    // 메모의 원래 크기에 곱할 배율입니다.
    UPROPERTY(EditAnywhere, Category = "Memo|Board",
        meta = (ClampMin = "0.1"))
    float FocusScaleMultiplier = 1.4f;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Memo|Board")
    TObjectPtr<UUserWidget> BoardWidget;

    // BP에서 생성된 위젯에 이 보드의 참조를 전달합니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Memo|Board")
    void SetupBoardWidget();

    // 메모 이동이 끝나면 호출됩니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Memo|Board")
    void ShowMemoDetails(const FText& Title, const FText& Body);

    UFUNCTION(BlueprintImplementableEvent, Category = "Memo|Board")
    void HideMemoDetails();

private:
    void OpenBoard(APlayerController* PC);
    void RestoreSelectedMemo();

    TWeakObjectPtr<APlayerController> ViewingPC;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    TWeakObjectPtr<AMemoPaperActor> SelectedMemo;

    FTransform OriginalMemoTransform;
    FTransform FocusMemoTransform;

    bool bBoardOpen = false;
    bool bHasSelection = false;
    bool bMemoArrived = false;
    bool bPreviousMouseCursor = false;

    float CameraWaitRemaining = 0.0f;
    float MemoMoveElapsed = 0.0f;
};