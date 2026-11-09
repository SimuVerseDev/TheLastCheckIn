#include "CCTV/CCTVPadWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"

void UCCTVPadWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Anim_PadUp)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &UCCTVPadWidget::HandlePadAnimFinished);
		BindToAnimationFinished(Anim_PadUp, FinishedEvent);
	}
	Image_Pad->SetVisibility(ESlateVisibility::Collapsed);
}

void UCCTVPadWidget::PlayPadUp()
{
	if (bAnimating || !Anim_PadUp)
	{
		return;
	}
	bAnimating = true;
	bGoingUp = true;
	Image_Pad->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlayAnimationForward(Anim_PadUp);
}

void UCCTVPadWidget::PlayPadDown()
{
	if (bAnimating || !Anim_PadUp)
	{
		return;
	}
	bAnimating = true;
	bGoingUp = false;
	Image_Pad->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Anim_Static)
	{
		PlayAnimation(Anim_Static);
	}
	PlayAnimationReverse(Anim_PadUp);
}

void UCCTVPadWidget::HandlePadAnimFinished()
{
	bAnimating = false;
	if (bGoingUp)
	{
		OnPadCovered.Broadcast();
		Image_Pad->SetVisibility(ESlateVisibility::Collapsed);
		if (Anim_PadUp)
		{
			PlayAnimation(Anim_Static);
		}
	}
	else
	{
		Image_Pad->SetVisibility(ESlateVisibility::Collapsed);
		OnPadLowered.Broadcast();
	}
}
