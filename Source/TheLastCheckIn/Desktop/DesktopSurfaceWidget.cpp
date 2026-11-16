#include "Desktop/DesktopSurfaceWidget.h"
#include "Desktop/DesktopWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/TextBlock.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "CheckIn/CheckInInteractionComponent.h"
#include "Rooms/RoomActor.h"
#include "Core/Economy/HotelEconomySubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Shop/ShopProduct.h"
#include "Shop/ShopPlacementComponent.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Editor.h"
#include "AssetCompilingManager.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "RenderingThread.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

namespace
{
	FSlateBrush Art(const TCHAR* Name)
	{
		FSlateBrush B;
		B.SetResourceObject(
			LoadObject<UTexture2D>(nullptr, *(FString(TEXT("/Game/Desktop/Art/")) + Name + TEXT(".") + Name)));
		B.DrawAs = ESlateBrushDrawType::Image;
		return B;
	}

	void Place(UCanvasPanel* P, UWidget* W, float X, float Y, float Width, float Height)
	{
		auto* S = P->AddChildToCanvas(W);
		S->SetPosition(FVector2D(X, Y));
		S->SetSize(FVector2D(Width, Height));
	}

	const TCHAR* MainNames[] = {TEXT("객실"),TEXT("구매"),TEXT("예약"),TEXT("관리"),TEXT("평판"),TEXT("매출"),TEXT("은행")};
	const TCHAR* SubNames[] = {
		TEXT("객실현황"),TEXT("투숙객목록"),TEXT("가구"),TEXT("도구"),TEXT("예약목록"),TEXT("예약완료"),TEXT("해금목록"),TEXT("직원관리"),
		TEXT("호텔평판"),TEXT("손님리뷰"),TEXT("전체현황"),TEXT("일일현황"),TEXT("대출"),TEXT("상환")
	};
}

void UDesktopActionButton::Activate() { if (Surface) Surface->HandleAction(Action); }

UImage* UDesktopSurfaceWidget::Picture(UCanvasPanel* P, const TCHAR* A, float X, float Y, float W, float H)
{
	auto* I = WidgetTree->ConstructWidget<UImage>();
	I->SetBrush(Art(A));
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(P, I, X, Y, W, H);
	return I;
}

UTextBlock* UDesktopSurfaceWidget::Label(UCanvasPanel* P, const FString& T, float X, float Y, float W, float H,
                                         int32 Size)
{
	auto* L = WidgetTree->ConstructWidget<UTextBlock>();
	L->SetText(FText::FromString(T));
	L->SetFont(FSlateFontInfo(
		LoadObject<UFont>(nullptr,TEXT("/Game/Desktop/Fonts/BMEuljiro10yearslater_Font.BMEuljiro10yearslater_Font")),
		Size));
	L->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(164, 106, 70))));
	L->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(P, L, X, Y, W, H);
	return L;
}

UDesktopActionButton* UDesktopSurfaceWidget::Button(UCanvasPanel* P, const FString& T, const TCHAR* A, int32 Id,
                                                    float X, float Y, float W, float H, int32 Size)
{
	auto* B = WidgetTree->ConstructWidget<UDesktopActionButton>();
	B->Action = Id;
	FButtonStyle S;
	S.SetNormal(Art(A));
	S.SetHovered(Art(A));
	S.SetPressed(Art(A));
	S.SetDisabled(Art(A));
	B->SetStyle(S);
	B->SetBackgroundColor(FLinearColor::White);
	Place(P, B, X, Y, W, H);
	auto* L = WidgetTree->ConstructWidget<UTextBlock>();
	L->SetText(FText::FromString(T));
	L->SetFont(FSlateFontInfo(
		LoadObject<UFont>(nullptr,TEXT("/Game/Desktop/Fonts/BMEuljiro10yearslater_Font.BMEuljiro10yearslater_Font")),
		Size));
	L->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(164, 106, 70))));
	B->AddChild(L);
	B->Surface = this;
	B->OnClicked.AddUniqueDynamic(B, &UDesktopActionButton::Activate);
	return B;
}

void UDesktopSurfaceWidget::Underline(UCanvasPanel* P, UTextBlock* L, float X, float Y)
{
	L->TakeWidget()->SlatePrepass();
	L->ForceLayoutPrepass();
	Picture(P,TEXT("NameLine"), X, Y, FMath::Max(40.f, L->GetDesiredSize().X + 24.f), 5);
}

void UDesktopSurfaceWidget::BuildLayout()
{
	MenuButtons.Reset();
	Page = nullptr;
	MoneyText = nullptr;
	ClockText = nullptr;
	if (MenuLabels.Num() != 7)
	{
		MenuLabels.Empty();
		for (auto* N : MainNames) MenuLabels.Add(FText::FromString(N));
	}
	if (SubMenuLabels.Num() != 14)
	{
		SubMenuLabels.Empty();
		for (auto* N : SubNames) SubMenuLabels.Add(FText::FromString(N));
	}
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("DesktopRoot"));
	WidgetTree->RootWidget = Root;
	if (LayoutMode == 0)
	{
		auto* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DesktopBlackBackground"));
		Background->SetBrushColor(FLinearColor::Black);
		auto* BackgroundSlot = Root->AddChildToCanvas(Background);
		BackgroundSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		BackgroundSlot->SetOffsets(FMargin(0));
		BackgroundSlot->SetZOrder(-100);
	}
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	auto* SS = Root->AddChildToCanvas(Scale);
	SS->SetAnchors(FAnchors(0, 0, LayoutMode == 2 ? 0.f : 1.f, 1));
	SS->SetOffsets(LayoutMode == 2 ? FMargin(24, 80, 430, 80) : FMargin(0));
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(LayoutMode == 2 ? 620 : (LayoutMode == 1 ? 1850 : 1920));
	Box->SetHeightOverride(LayoutMode == 2 ? 1000 : (LayoutMode == 1 ? 695 : 1080));
	Scale->AddChild(Box);
	auto* C = WidgetTree->ConstructWidget<UCanvasPanel>();
	Box->AddChild(C);
	if (LayoutMode != 1)
	{
		Picture(C,TEXT("Background"), 0, 0, LayoutMode == 2 ? 620 : 1920, LayoutMode == 2 ? 1000 : 1080);
		Label(C, LayoutMode == 2 ? TEXT("객실 배정") : HotelName.ToString(), 48, 20, 1100, 65, LayoutMode == 2 ? 36 : 45);
		Button(C,TEXT(""),TEXT("Close"), 99, LayoutMode == 2 ? 550 : 1820, 28, 55, 42);
	}
	if (LayoutMode == 0)
	{
		Picture(C,TEXT("Minimize"), 1705, 28, 55, 44);
		Picture(C,TEXT("Maximize"), 1763, 28, 54, 42);
		for (int32 I = 0; I < 7; ++I)
			MenuButtons.Add(Button(C, MenuLabels[I].ToString(),TEXT("Menu"), I, 398 + 213 * I,
			                       110, 200, 78, 43));
		for (int32 I = 0; I < 2; ++I)
			MenuButtons.Add(Button(C, SubMenuLabels[I].ToString(),TEXT("SubMenu"), 10 + I,
			                       378 + 260 * I, 216, 260, 58, 30));
		Picture(C,TEXT("Gold"), 80, 995, 54, 55);
		MoneyText = Label(C,TEXT("0"), 142, 995, 900, 60, 36);
		ClockText = Label(C,TEXT(""), 1480, 42, 220, 45, 22);
		MoneyText->Rename(TEXT("CapitalValue"));
		ClockText->Rename(TEXT("ClockValue"));
	}
	Page = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("PageContent"));
	Place(C, Page, LayoutMode == 2 ? 10 : (LayoutMode == 1 ? 0 : 35),
	      LayoutMode == 2 ? 105 : (LayoutMode == 1 ? 0 : 274), LayoutMode == 2 ? 600 : 1850,
	      LayoutMode == 2 ? 865 : 695);
	BuildPage();
}

UCheckInInteractionComponent* UDesktopSurfaceWidget::CheckIn() const
{
	auto* PC = GetOwningPlayer();
	return PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UCheckInInteractionComponent>() : nullptr;
}

void UDesktopSurfaceWidget::BuildPage()
{
	if (!Page) return;
	Page->ClearChildren();
	RoomDoors.Reset();
	ShopProductIds.Reset();
	ShopMessage = nullptr;
	ShopTable = nullptr;
	if (LayoutMode == 0)
	{
		for (int32 I = 0; I < MenuButtons.Num(); ++I)
		{
			auto* B = MenuButtons[I].Get();
			const bool Main = I < 7;
			const bool Selected = Main ? I == SelectedMenu : I - 7 == SelectedSub;
			const TCHAR* A = Main
				                 ? (Selected ? TEXT("MenuSelected") : TEXT("Menu"))
				                 : (Selected ? TEXT("SubMenuSelected") : TEXT("SubMenu"));
			FButtonStyle S = B->GetStyle();
			S.SetNormal(Art(A));
			S.SetHovered(Art(A));
			S.SetPressed(Art(A));
			B->SetStyle(S);
			if (!Main) Cast<UTextBlock>(B->GetChildAt(0))->SetText(SubMenuLabels[SelectedMenu * 2 + I - 7]);
		}
	}
	if (LayoutMode == 0 && (SelectedMenu >= 4 || (SelectedMenu == 3 && SelectedSub == 1)))
	{
		Picture(Page,TEXT("ComingSoon"), 326, 230, 1197, 235);
		return;
	}
	if (LayoutMode == 0 && SelectedMenu == 0 && SelectedSub == 0 && GetWorld() && GetWorld()->IsGameWorld())
	{
		auto Class = LoadClass<UDesktopSurfaceWidget>(
			nullptr,TEXT("/Game/Desktop/UI/WBP_RoomManagement.WBP_RoomManagement_C"));
		if (Class)
		{
			auto* RoomsWidget = CreateWidget<UDesktopSurfaceWidget>(GetOwningPlayer(), Class);
			Place(Page, RoomsWidget, 0, 0, 1850, 695);
			return;
		}
	}
	if (LayoutMode == 0 && SelectedMenu == 1)
	{
		BuildShopPage();
		return;
	}
	float Top = 0;
	if (LayoutMode == 0 && SelectedMenu == 3)
	{
		Button(Page,TEXT("구매"), UnlockTab == 0 ? TEXT("SubMenuSelected") : TEXT("SubMenu"), 20, 70, 50, 180, 64);
		Button(Page,TEXT("해금"), UnlockTab == 1 ? TEXT("SubMenuSelected") : TEXT("SubMenu"), 21, 265, 50, 180, 64);
		Top = 120;
	}
	auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetScrollbarThickness(FVector2D(16, 16));
	Place(Page, Scroll, 0, Top, LayoutMode == 2 ? 600 : 1850, (LayoutMode == 2 ? 865 : 695) - Top);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
	TArray<ARoomActor*> Rooms;
	if (GetWorld() && !IsDesignTime()) for (TActorIterator<ARoomActor> It(GetWorld()); It; ++It) Rooms.Add(*It);
	Rooms.Sort([](const ARoomActor& A, const ARoomActor& B) { return A.GetRoomNumber() < B.GetRoomNumber(); });
	const bool RoomPage = LayoutMode != 0 || (SelectedMenu == 0 && SelectedSub == 0);
	const bool UnlockPage = LayoutMode == 0 && SelectedMenu == 3;
	const bool Guests = LayoutMode == 0 && SelectedMenu == 0 && SelectedSub == 1;
	const bool Booking = LayoutMode == 0 && SelectedMenu == 2;
	if (RoomPage) Rooms.RemoveAll([](ARoomActor* R) { return !R->IsUnlocked(); });
	if (UnlockPage && UnlockTab == 0) Rooms.RemoveAll([](ARoomActor* R) { return R->IsUnlocked(); });
	if (UnlockPage && UnlockTab == 1) Rooms.RemoveAll([](ARoomActor* R) { return !R->IsUnlocked(); });
	const bool Preview = IsDesignTime() || !GetWorld() || !GetWorld()->IsGameWorld();
	int32 Count = (RoomPage || UnlockPage) ? (Preview ? 6 : Rooms.Num()) : (Guests ? 4 : 6);
	if (Booking && SelectedSub == 1) Count = 0;
	if (!Count)
	{
		Label(Page, Booking ? TEXT("완료된 예약이 없습니다") : TEXT("표시할 객실이 없습니다"), 60, Top + 40, 1400, 60, 30);
		return;
	}
	const int32 Cols = (LayoutMode == 2 || Guests) ? 1 : 3;
	for (int32 I = 0; I < Count;)
	{
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(10, Guests && I == 0 ? 28 : 20, 10, 5));
		for (int32 Col = 0; Col < Cols && I < Count; ++Col, ++I)
		{
			const float W = Guests ? 1800 : 600;
			const float H = Guests ? 144 : (UnlockPage ? 360 : 370);
			auto* Size = WidgetTree->ConstructWidget<USizeBox>();
			Size->SetWidthOverride(W);
			Size->SetHeightOverride(H);
			Row->AddChildToHorizontalBox(Size);
			auto* Card = WidgetTree->ConstructWidget<UCanvasPanel>();
			Size->AddChild(Card);
			ARoomActor* R = Rooms.IsValidIndex(I) ? Rooms[I] : nullptr;
			if (RoomPage)
			{
				const int32 Number = R ? R->GetRoomNumber() : 201 + I;
				const bool Assigned = R && R->GetRoomState() != ERoomState::Available;
				auto* B = Button(Card,TEXT(""),TEXT("RoomCard"), 1000 + Number, 0, 0, 593, 358);
				B->SetToolTipText(FText::FromString(TEXT("체크인 중 배정 가능한 객실을 선택하세요")));
				bool Exists = false, Occupied = false, Selectable = false;
				if (auto* CI = CheckIn()) CI->GetRoomStatus(Number, Exists, Occupied, Selectable);
				B->SetIsEnabled(Selectable || Preview);
				RoomDoors.Add(
					Number, Picture(Card, Assigned ? TEXT("DoorClosed") : TEXT("DoorOpen"), 220, 55, 146, 199));
				Label(Card, FString::Printf(TEXT("%d호"), Number), 30, 50, 180, 45, 25);
				Label(Card, FString::Printf(TEXT("%d인"), R ? R->GetCapacity() : 2), 120, 278, 100, 60, 30);
				Label(Card,TEXT("—"), 280, 278, 95, 60, 30);
				Label(Card, R ? FString::Printf(TEXT("%.0f"), R->GetBaseRoomRate()) : TEXT("—"), 440, 278, 120, 60, 30);
			}
			else if (Guests)
			{
				Picture(Card,TEXT("GuestCard"), 140, 0, 1516, 148);
				Picture(Card,TEXT("GuestImage"), 195, 25, 140, 96);
				auto* L = Label(Card,TEXT("이름"), 365, 35, 260, 60, 36);
				Underline(Card, L, 353, 112);
				Label(Card,TEXT("룸이름"), 710, 35, 400, 60, 36);
				// Reference layout: star, copper bar, then a dark inset behind the label.
				Picture(Card,TEXT("SatisfactionStar"), 1280, 32, 81, 78);
				Picture(Card,TEXT("SatisfactionBar"), 1365, 43, 264, 61);
				auto* SatisfactionInset = WidgetTree->ConstructWidget<UBorder>();
				SatisfactionInset->SetBrushColor(FLinearColor::FromSRGBColor(FColor(25, 29, 13)));
				SatisfactionInset->SetVisibility(ESlateVisibility::HitTestInvisible);
				Place(Card, SatisfactionInset, 1370, 48, 185, 50);
				auto* SatisfactionText = Label(Card,TEXT("만족도"), 1370, 45, 185, 61, 36);
				SatisfactionText->SetJustification(ETextJustify::Center);
			}
			else if (UnlockPage)
			{
				const int32 Number = R ? R->GetRoomNumber() : 205 + I;
				auto* B = Button(Card,TEXT(""),TEXT("UnlockCard"), 2000 + Number, 35, 0, 472, 357);
				B->SetIsEnabled(UnlockTab == 0 && R && !R->IsUnlocked());
				Picture(Card,TEXT("UnlockImage"), 75, 60, 229, 181);
				Label(Card, FString::Printf(TEXT("%d층"), Number / 100), 365, 75, 150, 55, 32);
				Label(Card, FString::Printf(TEXT("%d호"), Number), 355, 155, 180, 55, 30);
				Label(Card, UnlockTab == 1
					            ? TEXT("해금 완료")
					            : FString::Printf(TEXT("G %.0f"), R ? R->GetUnlockPrice() : 0), 100, 270, 380, 60, 30);
			}
			else if (Booking)
			{
				Picture(Card,TEXT("BookingCard"), 0, 0, 588, 357);
				Picture(Card,TEXT("BookingImage"), 30, 48, 204, 177);
				const TCHAR* Fields[] = {TEXT("이름"),TEXT("체크인"),TEXT("체크아웃"),TEXT("인원")};
				for (int J = 0; J < 4; ++J)
				{
					auto* L = Label(Card, Fields[J], 248, 40 + J * 52, 310, 40, 21);
					Underline(Card, L, 248, 80 + J * 52);
				}
				auto* L = Label(Card,TEXT("요구사항"), 40, 245, 400, 40, 22);
				Underline(Card, L, 40, 305);
			}
			else
			{
				Picture(Card,TEXT("PurchaseCard"), 50, 0, 473, 355);
				Picture(Card,TEXT("PurchaseImage"), 105, 40, 361, 199);
				Label(Card, SelectedSub == 0 ? TEXT("가구 이름") : TEXT("도구 이름"), 245, 245, 250, 50, 30);
				Label(Card,TEXT("G —"), 105, 295, 300, 45, 26);
			}
		}
	}
}

void UDesktopSurfaceWidget::BuildShopPage()
{
	ShopTable = GetDefault<UShopSettings>()->ProductTable.LoadSynchronous();
	const EShopProductCategory Category = SelectedSub == 0 ? EShopProductCategory::Furniture : EShopProductCategory::Tool;
	if (ShopTable && ShopTable->GetRowStruct() == FShopProductRow::StaticStruct())
	{
		for (const FName Id : ShopTable->GetRowNames())
		{
			const auto* Product = ShopTable->FindRow<FShopProductRow>(Id, TEXT("Shop"));
			if (Product && Product->bAvailable && Product->Category == Category) ShopProductIds.Add(Id);
		}
		ShopProductIds.Sort([this](FName A, FName B)
		{
			const auto* Left = ShopTable->FindRow<FShopProductRow>(A, TEXT("Shop"));
			const auto* Right = ShopTable->FindRow<FShopProductRow>(B, TEXT("Shop"));
			return Left->SortOrder == Right->SortOrder ? A.LexicalLess(B) : Left->SortOrder < Right->SortOrder;
		});
	}
	ShopMessage = Label(Page, ShopProductIds.IsEmpty() ? TEXT("등록된 상품이 없습니다") : TEXT("상품 선택 → 배치 확정 / Esc 취소·환불"), 60, 5, 1700, 45, 25);
	auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Place(Page, Scroll, 0, 55, 1850, 640);
	auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Rows);
	for (int32 I = 0; I < ShopProductIds.Num();)
	{
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(10, 20, 10, 5));
		for (int32 Col = 0; Col < 3 && I < ShopProductIds.Num(); ++Col, ++I)
		{
			const auto* Product = ShopTable->FindRow<FShopProductRow>(ShopProductIds[I], TEXT("Shop"));
			auto* Size = WidgetTree->ConstructWidget<USizeBox>();
			Size->SetWidthOverride(600);
			Size->SetHeightOverride(370);
			Row->AddChild(Size);
			auto* Card = WidgetTree->ConstructWidget<UCanvasPanel>();
			Size->AddChild(Card);
			auto* Buy = Button(Card, TEXT(""), TEXT("PurchaseCard"), 3000 + I, 50, 0, 473, 355);
			Buy->SetToolTipText(NSLOCTEXT("Shop", "BuyTooltip", "구매 후 배치를 확정하세요. Esc로 취소하면 전액 환불됩니다."));
			auto* Icon = Picture(Card, TEXT("PurchaseImage"), 105, 40, 361, 199);
			if (auto* Texture = Product->Icon.LoadSynchronous()) Icon->SetBrushFromTexture(Texture);
			Label(Card, Product->DisplayName.IsEmpty() ? ShopProductIds[I].ToString() : Product->DisplayName.ToString(), 105, 245, 360, 50, 30);
			Label(Card, FString::Printf(TEXT("G %d"), Product->Price), 105, 295, 300, 45, 26);
		}
	}
}

void UDesktopSurfaceWidget::BindButtons()
{
	WidgetTree->ForEachWidget([this](UWidget* W)
	{
		if (auto* B = Cast<UDesktopActionButton>(W))
		{
			B->Surface = this;
			B->OnClicked.AddUniqueDynamic(B, &UDesktopActionButton::Activate);
		}
	});
}

void UDesktopSurfaceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (LayoutMode >= 2)
	{
		BindButtons();
		if (GetWorld())
			GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UDesktopSurfaceWidget::RefreshData,
			                                       0.2f, true);
		return;
	}
	Page = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("PageContent")));
	MoneyText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CapitalValue")));
	ClockText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ClockValue")));
	if (!Page) BuildLayout();
	else
	{
		MenuButtons.Reset();
		WidgetTree->ForEachWidget([this](UWidget* W)
		{
			if (auto* B = Cast<UDesktopActionButton>(W))if (B->Action >= 0 && B->Action <= 11)MenuButtons.Add(B);
		});
		MenuButtons.Sort([](const UDesktopActionButton& A, const UDesktopActionButton& B)
		{
			return A.Action < B.Action;
		});
		BuildPage();
	}
	BindButtons();
	RefreshData();
	if (GetWorld())
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UDesktopSurfaceWidget::RefreshData,
		                                       0.5f, true);
}

void UDesktopSurfaceWidget::NativeDestruct()
{
	if (GetWorld())GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

void UDesktopSurfaceWidget::RefreshData()
{
	if (LayoutMode >= 2)
	{
		if (LayoutMode == 3)
		{
			auto* Assigned = FindFProperty<FBoolProperty>(GetClass(),TEXT("bAssigned"));
			if (auto* I = Cast<UImage>(WidgetTree->FindWidget(TEXT("RoomImage"))))
			{
				I->SetBrush(Art(Assigned && Assigned->GetPropertyValue_InContainer(this)
					                ? TEXT("DoorClosed")
					                : TEXT("DoorOpen")));
				I->SetColorAndOpacity(FLinearColor::White);
				I->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		return;
	}
	if (auto* W = GetWorld())
	{
		if (MoneyText)
			if (auto* E = W->GetSubsystem<UHotelEconomySubsystem>())
				MoneyText->SetText(
					FText::AsNumber(FMath::RoundToInt(E->GetCapital())));
		if (ClockText)
			if (auto* C = W->GetSubsystem<UGameClockSubsystem>())
				ClockText->SetText(
					FText::FromString(
						FString::Printf(TEXT("%d일차  %02d : %02d"), C->GetDay(), C->GetHour(), C->GetMinute())));
	}
	// Update room availability without rebuilding the scroll list or moving focus.
	if (WidgetTree)
		WidgetTree->ForEachWidget([this](UWidget* W)
		{
			if (auto* B = Cast<UDesktopActionButton>(W))
				if (B->Action >= 1000 && B->Action < 2000)
				{
					bool E = false, A = false, S = false;
					if (auto* CI = CheckIn())CI->GetRoomStatus(B->Action - 1000, E, A, S);
					B->SetIsEnabled(S);
					if (auto* Door = RoomDoors.Find(B->Action - 1000))
						(*Door)->SetBrush(
							Art(A ? TEXT("DoorClosed") : TEXT("DoorOpen")));
				}
		});
}

void UDesktopSurfaceWidget::SelectPage(int32 M, int32 S)
{
	SelectedMenu = FMath::Clamp(M, 0, 6);
	SelectedSub = FMath::Clamp(S, 0, 1);
	BuildPage();
}

void UDesktopSurfaceWidget::HandleAction(int32 A)
{
	if (A < 7 && A >= 0)
	{
		SelectPage(A, 0);
		return;
	}
	if (A == 10 || A == 11)
	{
		SelectPage(SelectedMenu, A - 10);
		return;
	}
	if (A == 20 || A == 21)
	{
		UnlockTab = A - 20;
		BuildPage();
		return;
	}
	if (A == 99)
	{
		if (LayoutMode == 2) { if (auto* CI = CheckIn())CI->CloseCheckIn(); }
		else if (auto* D = Cast<UDesktopWidget>(this))D->CloseDesktop();
		return;
	}
	if (A >= 1000 && A < 2000)
	{
		if (auto* CI = CheckIn())CI->TryAssignRoomNumber(A - 1000);
		return;
	}
	if (A >= 3000)
	{
		if (SelectedMenu != 1 || !ShopProductIds.IsValidIndex(A - 3000)) return;
		auto* PC = GetOwningPlayer();
		auto* Placement = PC ? PC->FindComponentByClass<UShopPlacementComponent>() : nullptr;
		FText Error = NSLOCTEXT("Shop", "NoPlacementComponent", "구매 기능을 사용할 수 없습니다.");
		if (!Placement || !Placement->PurchaseAndPlace(ShopTable, ShopProductIds[A - 3000], this, Error))
			if (ShopMessage) ShopMessage->SetText(Error);
		return;
	}
	if (A >= 2000 && A < 3000 && GetWorld())
	{
		ARoomActor* TargetRoom = nullptr;
		int32 Matches = 0;
		for (TActorIterator<ARoomActor> It(GetWorld()); It; ++It)
			if (It->GetRoomNumber() == A - 2000)
			{
				TargetRoom = *It;
				++Matches;
			}
		const bool Purchased = Matches == 1 && TargetRoom->TryPurchaseUnlock();
		if (Purchased)if (auto* CI = CheckIn())CI->RefreshRoomList();
		BuildPage();
		RefreshData();
		if (!Purchased) Label(Page, TEXT("해금할 수 없습니다. 잔액과 객실 상태를 확인해주세요."), 500, 65, 1200, 45, 24);
	}
}

FReply UDesktopSurfaceWidget::NativeOnPreviewKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	if (LayoutMode == 2 && E.GetKey() == EKeys::Escape)
	{
		HandleAction(99);
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(G, E);
}

bool UDesktopSurfaceWidget::InstallLayout(UObject* Object, int32 Mode)
{
#if WITH_EDITOR
	auto* BP = Cast<UWidgetBlueprint>(Object);
	if (!BP)return false;
	BP->Modify();
	if (Mode == 0 && BP->WidgetTree && BP->WidgetTree->FindWidget(TEXT("PageContent")))
	{
		// Upgrade the existing designer tree without replacing graphs or authored controls.
		auto* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
		if (!Root) return false;
		auto* Background = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("DesktopBlackBackground")));
		if (!Background)
		{
			Background = BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DesktopBlackBackground"));
			Root->AddChildToCanvas(Background);
			BP->OnVariableAdded(Background->GetFName());
		}
		Background->SetBrushColor(FLinearColor::Black);
		auto* BackgroundSlot = CastChecked<UCanvasPanelSlot>(Background->Slot);
		BackgroundSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		BackgroundSlot->SetOffsets(FMargin(0));
		BackgroundSlot->SetZOrder(-100);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		(void)BP->MarkPackageDirty();
		return BP->Status != BS_Error;
	}
	if (Mode >= 2)
	{
		// Keep every check-in graph, room number, binding, and original sidebar anchor.
		BP->ParentClass = StaticClass();
		BP->WidgetTree->ForEachWidget([](UWidget* W)
		{
			if (auto* T = Cast<UTextBlock>(W))
			{
				auto Font = T->GetFont();
				Font.FontObject = LoadObject<UFont>(
					nullptr,TEXT("/Game/Desktop/Fonts/BMEuljiro10yearslater_Font.BMEuljiro10yearslater_Font"));
				T->SetFont(Font);
				T->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(164, 106, 70))));
			}
			if (auto* I = Cast<UImage>(W))
			{
				if (I->GetName() == TEXT("SidebarBackground"))I->SetBrush(Art(TEXT("Background")));
				if (I->GetName() == TEXT("CardBackground"))I->SetBrush(Art(TEXT("RoomCard")));
				if (I->GetName() == TEXT("RoomImage"))I->SetBrush(Art(TEXT("DoorOpen")));
				I->SetColorAndOpacity(FLinearColor::White);
				I->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			if (auto* B = Cast<UBorder>(W))
			{
				B->SetBrush(Art(TEXT("SubMenu")));
				B->SetBrushColor(FLinearColor::White);
			}
		});
		if (Mode == 3)
		{
			auto* Tree = BP->WidgetTree.Get();
			auto* Outer = Cast<USizeBox>(Tree->FindWidget(TEXT("CardSize")));
			auto* ButtonWidget = Cast<UButton>(Tree->FindWidget(TEXT("RoomButton")));
			if (Outer && ButtonWidget)
			{
				const TCHAR* Names[] = {
					TEXT("CardBackground"),TEXT("RoomImage"),TEXT("RoomNumberText"),TEXT("RoomStatusText"),
					TEXT("RoomTypeText")
				};
				TArray<UWidget*> Saved;
				for (auto* N : Names)
				{
					auto* W = Tree->FindWidget(N);
					if (W)W->RemoveFromParent();
					Saved.Add(W);
				}
				ButtonWidget->RemoveFromParent();
				ButtonWidget->ClearChildren();
				Outer->ClearChildren();
				auto* Scale = Tree->ConstructWidget<UScaleBox>();
				Scale->SetStretch(EStretch::ScaleToFit);
				Outer->AddChild(Scale);
				auto* Size = Tree->ConstructWidget<USizeBox>();
				Size->SetWidthOverride(593);
				Size->SetHeightOverride(358);
				Scale->AddChild(Size);
				auto* Canvas = Tree->ConstructWidget<UCanvasPanel>();
				Size->AddChild(Canvas);
				FButtonStyle Style;
				Style.SetNormal(Art(TEXT("RoomCard")));
				Style.SetHovered(Art(TEXT("RoomCard")));
				Style.SetPressed(Art(TEXT("RoomCard")));
				Style.SetDisabled(Art(TEXT("RoomCard")));
				ButtonWidget->SetStyle(Style);
				Place(Canvas, ButtonWidget, 0, 0, 593, 358);
				if (Saved[0])
				{
					Place(Canvas, Saved[0], 0, 0, 593, 358);
					Saved[0]->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
				if (Saved[1])Place(Canvas, Saved[1], 224, 54, 146, 199);
				if (Saved[2])Place(Canvas, Saved[2], 28, 12, 170, 45);
				if (Saved[3])Place(Canvas, Saved[3], 390, 14, 170, 45);
				if (Saved[4])Place(Canvas, Saved[4], 100, 282, 150, 50);
			}
		}
		TSet<FName> WidgetNames;
		BP->ForEachSourceWidget([BP,&WidgetNames](UWidget* W)
		{
			WidgetNames.Add(W->GetFName());
			if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))BP->OnVariableAdded(W->GetFName());
		});
		for (auto It = BP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)if (!WidgetNames.Contains(It.Key()))It
			.RemoveCurrent();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		CastChecked<UDesktopSurfaceWidget>(BP->GeneratedClass->GetDefaultObject())->LayoutMode = Mode;
		(void)BP->MarkPackageDirty();
		return BP->Status != BS_Error;
	}
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	for (auto* G : Graphs)FBlueprintEditorUtils::RemoveGraph(BP, G);
	BP->Bindings.Empty();
	BP->NewVariables.Empty();
	BP->ParentClass = Mode == 0 ? UDesktopWidget::StaticClass() : StaticClass();
	if (BP->WidgetTree) BP->WidgetTree->Rename(nullptr, GetTransientPackage(),
	                                           REN_DontCreateRedirectors | REN_NonTransactional);
	BP->WidgetVariableNameToGuidMap.Empty();
	BP->WidgetTree = NewObject<UWidgetTree>(BP,TEXT("WidgetTree"), RF_Transactional);
	TStrongObjectPtr<UDesktopSurfaceWidget> Builder(NewObject<UDesktopSurfaceWidget>());
	Builder->WidgetTree = BP->WidgetTree;
	Builder->LayoutMode = Mode;
	Builder->BuildLayout();
	BP->WidgetTree->ForEachWidget([](UWidget* W)
	{
		if (auto* B = Cast<UDesktopActionButton>(W))
		{
			B->OnClicked.Clear();
			B->Surface = nullptr;
		}
	});
	TSet<FName> WidgetNames;
	BP->ForEachSourceWidget([BP,&WidgetNames](UWidget* W)
	{
		WidgetNames.Add(W->GetFName());
		if (!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))BP->OnVariableAdded(W->GetFName());
	});
	for (auto It = BP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)if (!WidgetNames.Contains(It.Key()))It.
		RemoveCurrent();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	auto* Defaults = Cast<UDesktopSurfaceWidget>(BP->GeneratedClass->GetDefaultObject());
	Defaults->LayoutMode = Mode;
	Defaults->MenuLabels = Builder->MenuLabels;
	Defaults->SubMenuLabels = Builder->SubMenuLabels;
	(void)BP->MarkPackageDirty();
	return BP->Status != BS_Error;
#else
	return false;
#endif
}


bool UDesktopSurfaceWidget::RenderLayout(UObject* Object, const FString& Filename, int32 Menu, int32 SubMenu)
{
#if WITH_EDITOR
	auto* BP = Cast<UWidgetBlueprint>(Object);
	if (!BP || !GEditor || !FSlateApplication::IsInitialized())return false;
	auto* W = CreateWidget<UDesktopSurfaceWidget>(GEditor->GetEditorWorldContext().World(), BP->GeneratedClass.Get());
	if (!W)return false;
	auto Slate = W->TakeWidget();
	W->SelectPage(Menu, SubMenu);
	W->ForceLayoutPrepass();
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.EnableGlobalInvalidation")))CVar->Set(0);
	++GFrameCounter;
	Slate->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	FWidgetRenderer Renderer(false);
	auto* Target = Renderer.DrawWidget(Slate, FVector2D(1920, 1080));
	FlushRenderingCommands();
	TArray<FColor> Pixels;
	FReadSurfaceDataFlags Flags(RCM_UNorm);
	Flags.SetLinearToGamma(false);
	bool Result = Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
	TArray64<uint8> Bytes;
	if (Result)
	{
		for (FColor& Pixel : Pixels) Pixel = Pixel.ReinterpretAsLinear().ToFColorSRGB();
		FImageUtils::PNGCompressImageArray(1920, 1080, Pixels, Bytes);
		Result = FFileHelper::SaveArrayToFile(Bytes, *Filename);
	}
	W->RemoveFromParent();
	return Result;
#else
	return false;
#endif
}
