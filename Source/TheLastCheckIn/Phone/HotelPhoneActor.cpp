#include "Phone/HotelPhoneActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"

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
	return bRinging;
}

FText AHotelPhoneActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return AnswerPrompt;
}

void AHotelPhoneActor::Interact_Implementation(AActor* Interactor)
{
	if (!bRinging)
	{
		return;
	}
	StopRinging();
	// TODO : 3단계 - 카메라 고정 + 조작 막기
}
