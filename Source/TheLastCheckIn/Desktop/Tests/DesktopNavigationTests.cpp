#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Desktop/DesktopSurfaceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "Components/Image.h"
#include "UObject/UnrealType.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDesktopNavigationTest, "TheLastCheckIn.Desktop.Navigation",
                                 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FDesktopNavigationTest::RunTest(const FString&)
{
	auto Class = LoadClass<UDesktopSurfaceWidget>(nullptr,TEXT("/Game/Desktop/UI/WBP_Desktop.WBP_Desktop_C"));
	if (!TestNotNull(TEXT("Desktop asset loads"), Class))return false;
	auto* W = CreateWidget<UDesktopSurfaceWidget>(GEditor->GetEditorWorldContext().World(), Class);
	W->TakeWidget();
	for (int32 M = 0; M < 7; ++M)
		for (int32 S = 0; S < 2; ++S)
		{
			W->SelectPage(M, S);
			int32 MainSelected = 0, SubSelected = 0;
			bool ComingSoon = false;
			W->WidgetTree->ForEachWidget([&](UWidget* Child)
			{
				if (auto* B = Cast<UDesktopActionButton>(Child))
				{
					auto* Art = B->GetStyle().Normal.GetResourceObject();
					if (Art && Art->GetName() == TEXT("MenuSelected"))
					{
						++MainSelected;
						TestEqual(TEXT("Selected main survives release"), B->Action, M);
					}
					if (Art && Art->GetName() == TEXT("SubMenuSelected") && B->Action < 20)
					{
						++SubSelected;
						TestEqual(TEXT("Selected submenu survives release"), B->Action, 10 + S);
					}
				}
				if (auto* I = Cast<UImage>(Child))
				{
					auto* Art = I->GetBrush().GetResourceObject();
					ComingSoon |= Art && Art->GetName() == TEXT("ComingSoon");
				}
			});
			TestEqual(TEXT("Exactly one selected main"), MainSelected, 1);
			TestEqual(TEXT("Exactly one selected submenu"), SubSelected, 1);
			TestEqual(TEXT("Only unimplemented pages show Coming Soon"), ComingSoon, M >= 4 || (M == 3 && S == 1));
		}
	W->RemoveFromParent();
	return true;
}
#endif
