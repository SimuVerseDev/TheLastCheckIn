#include "Phone/HotelPhoneActor.h"
#include "Phone/UI/PhoneSubtitleWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AHotelPhoneActor::AHotelPhoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	RingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RingAudio"));
	AnswerPrompt = NSLOCTEXT("Phone", "Answer", "전화받기");
	
	ViewCamera->SetupAttachment(GetRootComponent());
	RingAudio->SetupAttachment(GetRootComponent());
	RingAudio->bAutoActivate = false;
}

void AHotelPhoneActor::StartRinging(const FText& InSubtitle)
{
	PendingSubtitle = InSubtitle;
	bRinging = true;
	if (RingSound)
	{
		RingAudio->SetSound(RingSound);
		RingAudio->Play();
	}
}

void AHotelPhoneActor::StopRinging()
{
	bRinging = false;
	RingAudio->Stop();
}

bool AHotelPhoneActor::CanInteract_Implementation(AActor* Interactor) const
{
	return bRinging && !CallerPC.IsValid();
}

FText AHotelPhoneActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return AnswerPrompt;
}

void AHotelPhoneActor::Interact_Implementation(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!bRinging || !PC || CallerPC.IsValid())
	{
		return;
	}
	StopRinging();
	CallerPC = PC;
	
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetViewTargetWithBlend(this, CameraBlendTime, VTBlend_EaseInOut, 2.0f);
	
	/*
	UI
	*/
	SubtitleWidget = CreateWidget<UPhoneSubtitleWidget>(PC, UPhoneSubtitleWidget::StaticClass());
	if (SubtitleWidget)
	{
		SubtitleWidget->SetSubtitle(PendingSubtitle);
		SubtitleWidget->AddToViewport(60);
	}
	
	GetWorldTimerManager().SetTimer(CallTimer, this, &ThisClass::EndCall, CallDuration, false);
}

/*
통화 끝 - 플레이어 시점으로 돌아가고 조작 복구
*/
void AHotelPhoneActor::EndCall()
{
	/*
	UI
	*/
	if (SubtitleWidget)
	{
		SubtitleWidget->RemoveFromParent();
		SubtitleWidget = nullptr;
	}
	
	APlayerController* PC = CallerPC.Get();
	CallerPC.Reset();
	if (!PC)
	{
		return;
	}
	PC->SetViewTargetWithBlend(PC->GetPawn(), CameraBlendTime, VTBlend_EaseInOut, 2.0f);
	PC->SetIgnoreMoveInput(false);
	PC->SetIgnoreLookInput(false);
}