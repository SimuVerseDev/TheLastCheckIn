#include "Core/Reputation/UI/ReputationHUDSubsystem.h"
#include "Core/Reputation/UI/ReputationHUDWidget.h"
#include "Core/Reputation/ReputationSettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UReputationHUDSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld())
	{
		InWorld.GetTimerManager().SetTimerForNextTick(this, &UReputationHUDSubsystem::CreateHUD);
	}
}

void UReputationHUDSubsystem::Deinitialize()
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	Super::Deinitialize();
}

void UReputationHUDSubsystem::CreateHUD()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalPlayerController() || HUDWidget)
	{
		return;
	}

	TSubclassOf<UReputationHUDWidget> WidgetClass = UReputationSettings::Get()->HUDWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		WidgetClass = UReputationHUDWidget::StaticClass();
	}

	HUDWidget = CreateWidget<UReputationHUDWidget>(PC, WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToPlayerScreen(0);
	}
}

void UReputationHUDSubsystem::SetHUDVisible(bool bVisible)
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
