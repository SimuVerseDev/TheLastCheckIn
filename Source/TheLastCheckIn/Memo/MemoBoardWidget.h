#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MemoBoardWidget.generated.h"

class AMemoBoardActor;
class UButton;
class UWidget;
class UTextBlock;
class UScrollBox;

/** Editable UMG presentation; input and memo data are connected by the board. */
UCLASS(Abstract)
class THELASTCHECKIN_API UMemoBoardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeBoard(AMemoBoardActor* InBoard);
    bool DisplayMemo(const FText& Title, const FText& Body);
    void HideMemo();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MemoDetails;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Title;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Body;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Back;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> Scroll_Body;

private:
    UFUNCTION()
    void HandleBack();
    UPROPERTY(Transient)
    TObjectPtr<AMemoBoardActor> BoardReference;
};
