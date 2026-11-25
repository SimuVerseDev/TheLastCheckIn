// Copyright Epic Games, Inc. All Rights Reserved.


#include "TheLastCheckInPlayerController.h"
#include "Core/UI/UILayers.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "TheLastCheckInCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "TheLastCheckIn.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Night/NightFlowComponent.h"
#include "Shop/ShopPlacementComponent.h"

ATheLastCheckInPlayerController::ATheLastCheckInPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ATheLastCheckInCameraManager::StaticClass();
	NightFlow = CreateDefaultSubobject<UNightFlowComponent>(TEXT("NightFlow"));
	ShopPlacement = CreateDefaultSubobject<UShopPlacementComponent>(TEXT("ShopPlacement"));
}

void ATheLastCheckInPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController() && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 2.0f, FLinearColor::Black, false, false);
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(UILayer::MobileControls);

		} else {

			UE_LOG(LogTheLastCheckIn, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ATheLastCheckInPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ATheLastCheckInPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
