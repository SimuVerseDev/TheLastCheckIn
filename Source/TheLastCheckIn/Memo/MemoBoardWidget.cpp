#include "Memo/MemoBoardWidget.h"
#include "Memo/MemoBoardActor.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UMemoBoardWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    if (BTN_Back)
    {
        BTN_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
    }
    HideMemo();
}

void UMemoBoardWidget::NativeDestruct()
{
    if (BTN_Back)
    {
        BTN_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBack);
    }
    BoardReference = nullptr;
    Super::NativeDestruct();
}

void UMemoBoardWidget::InitializeBoard(AMemoBoardActor* InBoard)
{
    BoardReference = InBoard;
}

bool UMemoBoardWidget::DisplayMemo(const FText& Title, const FText& Body)
{
    if (!MemoDetails || !TXT_Title || !TXT_Body)
    {
        return false;
    }
    TXT_Title->SetText(Title);
    TXT_Body->SetText(Body);
    if (Scroll_Body)
    {
        Scroll_Body->ScrollToStart();
    }
    MemoDetails->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    return true;
}

void UMemoBoardWidget::HideMemo()
{
    if (MemoDetails)
    {
        MemoDetails->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UMemoBoardWidget::HandleBack()
{
    if (IsValid(BoardReference))
    {
        BoardReference->Back();
    }
}

FReply UMemoBoardWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::LeftMouseButton && IsValid(BoardReference))
    {
        BoardReference->SelectMemoUnderCursor();
        return FReply::Handled();
    }
    if (Event.GetEffectingButton() == EKeys::RightMouseButton && IsValid(BoardReference))
    {
        HandleBack();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UMemoBoardWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if ((Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::E) && IsValid(BoardReference))
    {
        HandleBack();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
