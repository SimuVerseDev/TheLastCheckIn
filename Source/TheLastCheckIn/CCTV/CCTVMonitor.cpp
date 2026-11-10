#include "CCTV/CCTVMonitor.h"
#include "CCTV/CCTVPadWidget.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"

ACCTVMonitor::ACCTVMonitor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}


bool ACCTVMonitor::CanInteract_Implementation(AActor* Interactor) const
{
	if (bTransitioning)
	{
		return false;
	}
	if (bPadOpen)
	{
		return true;
	}
	const bool bHasValidCamera = Cameras.ContainsByPredicate(
		[](const TObjectPtr<ACameraActor>& Camera) { return IsValid(Camera); });
	return PadWidgetClass && bHasValidCamera;
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
		if (!PadWidget)
		{
			return;
		}
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
	UnbindCCTVInput();
}

void ACCTVMonitor::HandlePadCovered()
{
	bTransitioning = false;
	if (Cameras.IsValidIndex(CurrentCameraIndex) && IsValid(Cameras[CurrentCameraIndex]))
	{
		ApplyCurrentCamera();
	}
	else
	{
		SwitchCamera(1);
	}
	if (APlayerController* PC = ViewingPC.Get())
	{
		BindCCTVInput(PC);
	}
}

void ACCTVMonitor::HandlePadLowered()
{
	bTransitioning = false;
	UnbindCCTVInput();
	if (APlayerController* PC = ViewingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	ViewingPC.Reset();
}

void ACCTVMonitor::ApplyCurrentCamera() const
{
	APlayerController* PC = ViewingPC.Get();
	if (PC && Cameras.IsValidIndex(CurrentCameraIndex) && IsValid(Cameras[CurrentCameraIndex]))
	{
		PC->SetViewTargetWithBlend(Cameras[CurrentCameraIndex], 0.0f);
	}
}

void ACCTVMonitor::BindCCTVInput(APlayerController* PC)
{
	if (!PC || !CCTVMappingContext || !SwitchCameraAction || InputPC.IsValid())
	{
		return;
	}
	if (!CCTVInput)
	{
		CCTVInput = NewObject<UEnhancedInputComponent>(this, TEXT("CCTVInput"));
		CCTVInput->RegisterComponent();
		CCTVInput->BindAction(SwitchCameraAction, ETriggerEvent::Started, this, &ACCTVMonitor::HandleSwitchCamera);
	}
	PC->PushInputComponent(CCTVInput);
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(CCTVMappingContext, CCTVMappingPriority);
	}
	InputPC = PC;
}

void ACCTVMonitor::UnbindCCTVInput()
{
	APlayerController* PC = InputPC.Get();
	if (!PC)
	{
		return;
	}
	PC->PopInputComponent(CCTVInput);
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(CCTVMappingContext);
	}
	InputPC.Reset();
}

void ACCTVMonitor::HandleSwitchCamera(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (bTransitioning || FMath::IsNearlyZero(Axis))
	{
		return;
	}
	SwitchCamera(Axis > 0.f ? 1 : -1);
}

void ACCTVMonitor::SwitchCamera(int32 Direction)
{
	const int32 Num = Cameras.Num();
	if (Num == 0)
	{
		return;
	}
	for (int32 Step = 1; Step <= Num; ++Step)
	{
		/*
		다음 인덱스 = ((현재 번호 + 방향 × 몇 칸째) ÷ 카메라 수의 나머지 + 카메라 수) ÷ 카메라 수의 나머지
		*/
		const int32 Index = ((CurrentCameraIndex + Direction * Step) % Num + Num) % Num;
		if (IsValid(Cameras[Index]))
		{
			CurrentCameraIndex = Index;
			ApplyCurrentCamera();
			return;
		}
	}
}

/*
의도하지않은 경로로 모니터 액터가 파괴될 경우 크래시 방지
*/
void ACCTVMonitor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCCTVInput();
	
	if (APlayerController* PC = ViewingPC.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		PC->SetViewTargetWithBlend(PC->GetPawn(), 0.0f);
	}
	ViewingPC.Reset();
	
	if (PadWidget)
	{
		PadWidget->OnPadCovered.RemoveAll(this);
		PadWidget->OnPadLowered.RemoveAll(this);
		PadWidget->RemoveFromParent();
		PadWidget = nullptr;
	}
	
	bPadOpen = false;
	bTransitioning = false;
	Super::EndPlay(EndPlayReason);
}