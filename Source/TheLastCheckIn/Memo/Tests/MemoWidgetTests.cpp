#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Memo/MemoBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Editor.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemoReaderWidgetTest, "TheLastCheckIn.Memo.ReaderWidget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMemoReaderWidgetTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UMemoBoardWidget>(nullptr,
        TEXT("/Game/Memo/UI/WBP_MemoReader_01.WBP_MemoReader_01_C"));
    if (!TestNotNull(TEXT("Reader blueprint loads"), Class)) return false;
    UMemoBoardWidget* UI = CreateWidget<UMemoBoardWidget>(GEditor->GetEditorWorldContext().World(), Class);
    if (!TestNotNull(TEXT("Reader instance created"), UI)) return false;
    TSharedRef<SWidget> Slate = UI->TakeWidget();
    FAssetCompilingManager::Get().FinishAllCompilation();
    FlushRenderingCommands();
    UWidget* Details = UI->WidgetTree->FindWidget(TEXT("MemoDetails"));
    if (!TestNotNull(TEXT("Details bound"), Details)) return false;
    TestTrue(TEXT("Board initially exposes the 3D notes"), Details->GetVisibility() == ESlateVisibility::Collapsed);
    const FText Title = NSLOCTEXT("MemoTest", "Title", "첫 근무자를 위한 메모");
    const FText Body = NSLOCTEXT("MemoTest", "Body", "야간 근무를 시작하기 전에 메모보드를 확인하세요.\n\n로비의 조명이 깜빡이거나 낯선 소리가 들리면 주변을 살펴보세요.\n\n이 문장은 미리보기용입니다. 게임에서는 데이터 테이블의 내용이 표시됩니다.");
    TestTrue(TEXT("Display succeeds with actual widget bindings"), UI->DisplayMemo(Title, Body));
    auto* TitleWidget = Cast<UTextBlock>(UI->WidgetTree->FindWidget(TEXT("TXT_Title")));
    auto* BodyWidget = Cast<UTextBlock>(UI->WidgetTree->FindWidget(TEXT("TXT_Body")));
    TestTrue(TEXT("Title populated"), TitleWidget && TitleWidget->GetText().EqualTo(Title));
    TestTrue(TEXT("Body populated"), BodyWidget && BodyWidget->GetText().EqualTo(Body));
    auto* Back = Cast<UButton>(UI->WidgetTree->FindWidget(TEXT("BTN_Back")));
    TestTrue(TEXT("Back button wired"), Back && Back->OnClicked.IsBound());

    FWidgetRenderer* Renderer = new FWidgetRenderer(true, true);
    UTextureRenderTarget2D* Target = Renderer->DrawWidget(Slate, FVector2D(1280,720));
    if (TestNotNull(TEXT("Preview render target"), Target))
    {
        Renderer->DrawWidget(Target, Slate, FVector2D(1280,720), 0.0f);
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        if (TestTrue(TEXT("Preview readback"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)))
        {
            // The render target has transparent game-world background; use opaque pixels for inspection.
            for (FColor& Pixel : Pixels) Pixel.A = 255;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(1280,720,Pixels,Png);
            TestTrue(TEXT("Preview saved"), FFileHelper::SaveArrayToFile(Png,
                *(FPaths::ProjectSavedDir() / TEXT("MemoWidgetWork/Preview.png"))));
        }
    }
    BeginCleanup(Renderer);
    UI->HideMemo();
    TestTrue(TEXT("Reading overlay can close"), Details->GetVisibility() == ESlateVisibility::Collapsed);
    UI->RemoveFromParent();
    return true;
}
#endif
