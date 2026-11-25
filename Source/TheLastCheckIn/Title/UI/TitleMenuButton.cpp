#include "Title/UI/TitleMenuButton.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UTitleMenuButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Label)
	{
		Label->SetText(LabelText);
	}
	SetHighlighted(false);
}

void UTitleMenuButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (MenuButton)
	{
		MenuButton->OnHovered.AddDynamic(this, &ThisClass::HandleHovered);
		MenuButton->OnUnhovered.AddDynamic(this, &ThisClass::HandleUnhovered);
		MenuButton->OnClicked.AddDynamic(this, &ThisClass::HandleClicked);
	}
}

void UTitleMenuButton::HandleHovered()
{
	SetHighlighted(true);
	if (HoverSound)
	{
		UGameplayStatics::PlaySound2D(this, HoverSound);
	}
}

void UTitleMenuButton::HandleUnhovered()
{
	SetHighlighted(false);
}

void UTitleMenuButton::HandleClicked()
{
	if (ClickSound)
	{
		UGameplayStatics::PlaySound2D(this, ClickSound);
	}
	OnMenuClicked.Broadcast();
}

void UTitleMenuButton::SetHighlighted(bool bOn)
{
	const bool bEnabled = GetIsEnabled();
	const bool bLit = bOn && bEnabled;

	if (Label)
	{
		Label->SetColorAndOpacity(!bEnabled ? DisabledColor : (bLit ? HoverColor : NormalColor));

		FSlateFontInfo Font = Label->GetFont();
		Font.OutlineSettings.OutlineSize = bLit ? 2 : 0;
		Font.OutlineSettings.OutlineColor = GlowColor;
		Label->SetFont(Font);
	}
	if (Arrow)
	{
		Arrow->SetVisibility(bLit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	SetRenderScale(FVector2D(bLit ? 1.05f : 1.0f));
}