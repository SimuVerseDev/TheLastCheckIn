#include "CCTV/CCTVMonitor.h"
#include "CCTV/CCTVPadWidget.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"

ACCTVMonitor::ACCTVMonitor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

bool ACCTVMonitor::CanInteract_Implementation(AActor* Interactor) const
{
	return PadWidgetClass && Cameras.Num() > 0 && !bTransitioning;
}

FText ACCTVMonitor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return bPadOpen
		? NSLOCTEXT("CCTV", "Close", "CCTV 끄기")
		: NSLOCTEXT("CCTV", "Open", "CCTV 보기");
}

void ACCTVMonitor::Interact_Implementation(AActor* Interactor)
{
	if (bTransitioning)
	{
		return;
	}
	if (bPadOpen)
	{
		ClosePad();
		return;
	}
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		OpenPad(PC);
	}
}

bool ACCTVMonitor::IsInteractionActive_Implementation(AActor* Interactor) const
{
	return bPadOpen;
}

void ACCTVMonitor::OpenPad(APlayerController* PC)
{
	if (!PadWidget)
	{
		PadWidget = CreateWidget<UCCTVPadWidget>(PC, PadWidgetClass);
		PadWidget->OnPadCovered.AddDynamic(this, &ACCTVMonitor::HandlePadCovered);
		PadWidget->OnPadLowered.AddDynamic(this, &ACCTVMonitor::HandlePadLowered);
		PadWidget->AddToViewport();
	}
	bPadOpen = true;
	bTransitioning = true;
	
	ViewingPC = PC;
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PadWidget->PlayPadUp();
}

void ACCTVMonitor::ClosePad()
{
	APlayerController* PC = ViewingPC.Get();
	if (!PC || !PadWidget)
	{
		return;
	}
	bPadOpen = false;
	bTransitioning = true;
	
	PC->SetViewTargetWithBlend(PC->GetPawn(), 0.0f);
	PadWidget->PlayPadDown();
}

void ACCTVMonitor::HandlePadCovered()
{
	bTransitioning = false;
	APlayerController* PC = ViewingPC.Get();
	if (PC && Cameras.IsValidIndex(CurrentCameraIndex) && Cameras[CurrentCameraIndex])
	{
		PC->SetViewTargetWithBlend(Cameras[CurrentCameraIndex], 0.0f);
	}
}

void ACCTVMonitor::HandlePadLowered()
{
	bTransitioning = false;
	if (APlayerController* PC = ViewingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	ViewingPC.Reset();
}
