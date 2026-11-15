#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "DesktopSurfaceWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UImage;
class UCheckInInteractionComponent;
class UDesktopSurfaceWidget;

UCLASS()
class THELASTCHECKIN_API UDesktopActionButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UDesktopSurfaceWidget> Surface;
	UPROPERTY(EditAnywhere, Category="Desktop")
	int32 Action = -1;
	UFUNCTION()
	void Activate();
};

/** Shared PNG layout for desktop, room data, and the existing check-in sidebar. */
UCLASS()
class THELASTCHECKIN_API UDesktopSurfaceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 0: desktop shell, 1: room page, 2: check-in sidebar
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Desktop")
	int32 LayoutMode = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Desktop")
	FText HotelName = FText::FromString(TEXT("DIVINE LORRAINE"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Desktop")
	TArray<FText> MenuLabels;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Desktop")
	TArray<FText> SubMenuLabels;
	UFUNCTION(BlueprintCallable, Category="Desktop")
	void SelectPage(int32 Menu, int32 SubMenu);
	UFUNCTION(BlueprintCallable, Category="Desktop")
	void RefreshData();
	UFUNCTION(BlueprintCallable, Category="Desktop|Editor")
	static bool InstallLayout(UObject* Blueprint, int32 Mode);
	UFUNCTION(BlueprintCallable, Category="Desktop|Editor")
	static bool RenderLayout(UObject* Blueprint, const FString& Filename, int32 Menu, int32 SubMenu);
	void HandleAction(int32 Action);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;

private:
	void BuildLayout();
	void BuildPage();
	void BindButtons();
	UCheckInInteractionComponent* CheckIn() const;
	UImage* Picture(UCanvasPanel* Parent, const TCHAR* Asset, float X, float Y, float W, float H);
	UTextBlock* Label(UCanvasPanel* Parent, const FString& Text, float X, float Y, float W, float H, int32 Size = 30);
	UDesktopActionButton* Button(UCanvasPanel* Parent, const FString& Text, const TCHAR* Asset, int32 Action, float X,
	                             float Y, float W, float H, int32 Size = 32);
	void Underline(UCanvasPanel* Parent, UTextBlock* Text, float X, float Y);
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Page;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDesktopActionButton>> MenuButtons;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UImage>> RoomDoors;
	int32 SelectedMenu = 0, SelectedSub = 0, UnlockTab = 0;
	FTimerHandle RefreshTimer;
};
