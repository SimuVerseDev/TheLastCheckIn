#include "Phone/HotelPhoneActor.h"

#include "Core/UI/UILayers.h"
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

	SetRootComponent(
		CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(
		TEXT("ViewCamera"));

	RingAudio = CreateDefaultSubobject<UAudioComponent>(
		TEXT("RingAudio"));

	AnswerPrompt = NSLOCTEXT("Phone", "Answer", "전화받기");

	ViewCamera->SetupAttachment(GetRootComponent());
	RingAudio->SetupAttachment(GetRootComponent());
	RingAudio->bAutoActivate = false;
}

// 기존 BP용 요청: 요청자 없는 전화
void AHotelPhoneActor::StartRinging(const FText& InSubtitle)
{
	if (bEndingPlay)
	{
		return;
	}

	FHotelPhoneCallRequest Request;
	Request.Subtitle = InSubtitle;

	PendingCalls.Add(MoveTemp(Request));
	TryStartNextCall();
}

// 기존 C++ 호출을 새로운 접수 함수에 연결합니다.
void AHotelPhoneActor::StartRingingFor(
	const UObject* InRequester,
	const FText& InSubtitle)
{
	RequestCallFor(InRequester, InSubtitle);
}

bool AHotelPhoneActor::RequestCallFor(
	const UObject* InRequester,
	const FText& InSubtitle)
{
	if (bEndingPlay || !IsValid(InRequester))
	{
		return false;
	}

	const bool bSameCurrentRequester =
		CurrentCall.bHasRequester
		&& CurrentCall.Requester.Get() == InRequester;

	// 이미 통화 중인 같은 요청은 중복으로 접수하지 않습니다.
	if (bInCall && bSameCurrentRequester)
	{
		return false;
	}

	// 같은 요청자의 전화가 울리고 있다면 대사만 갱신합니다.
	if (bRinging && bSameCurrentRequester)
	{
		CurrentCall.Subtitle = InSubtitle;
		return true;
	}

	// 대기 중인 같은 요청도 중복 추가하지 않고 대사만 갱신합니다.
	for (FHotelPhoneCallRequest& Request : PendingCalls)
	{
		if (Request.bHasRequester
			&& Request.Requester.Get() == InRequester)
		{
			Request.Subtitle = InSubtitle;
			return true;
		}
	}

	FHotelPhoneCallRequest Request;
	Request.Requester = InRequester;
	Request.Subtitle = InSubtitle;
	Request.bHasRequester = true;

	PendingCalls.Add(MoveTemp(Request));
	TryStartNextCall();

	return true;
}

bool AHotelPhoneActor::HasCallFor(
	const UObject* InRequester) const
{
	if (!IsValid(InRequester))
	{
		return false;
	}

	if ((bRinging || bInCall)
		&& CurrentCall.bHasRequester
		&& CurrentCall.Requester.Get() == InRequester)
	{
		return true;
	}

	for (const FHotelPhoneCallRequest& Request : PendingCalls)
	{
		if (Request.bHasRequester
			&& Request.Requester.Get() == InRequester)
		{
			return true;
		}
	}

	return false;
}

// 요청자가 지정되지 않은 기존 취소 함수.
// 현재 벨소리만 취소하고 다음 대기 전화를 시작합니다.
void AHotelPhoneActor::StopRinging()
{
	if (!bRinging)
	{
		return;
	}

	StopCurrentRing();
	CurrentCall = FHotelPhoneCallRequest();

	TryStartNextCall();
}

// 해당 요청자의 미수신 전화만 취소합니다.
// 이미 받은 통화는 끝까지 진행합니다.
void AHotelPhoneActor::StopRingingFor(
	const UObject* InRequester)
{
	if (!InRequester)
	{
		return;
	}

	PendingCalls.RemoveAll(
		[InRequester](const FHotelPhoneCallRequest& Request)
		{
			return Request.bHasRequester
				&& Request.Requester.Get() == InRequester;
		});

	if (bRinging
		&& CurrentCall.bHasRequester
		&& CurrentCall.Requester.Get() == InRequester)
	{
		StopCurrentRing();
		CurrentCall = FHotelPhoneCallRequest();
	}

	TryStartNextCall();
}

// 대기열에서 다음 유효한 요청을 꺼내 벨소리를 시작합니다.
void AHotelPhoneActor::TryStartNextCall()
{
	if (bEndingPlay || IsBusy())
	{
		return;
	}

	while (!PendingCalls.IsEmpty())
	{
		FHotelPhoneCallRequest Request =
			MoveTemp(PendingCalls[0]);

		PendingCalls.RemoveAt(0);

		// 대기 중 요청자가 제거됐다면 건너뜁니다.
		if (Request.bHasRequester && !Request.Requester.IsValid())
		{
			continue;
		}

		CurrentCall = MoveTemp(Request);
		bRinging = true;

		if (RingSound)
		{
			RingAudio->SetSound(RingSound);
			RingAudio->Play();
		}

		return;
	}
}

// 벨소리만 멈춥니다. 다음 전화를 시작하거나 현재 요청을 지우지 않습니다.
void AHotelPhoneActor::StopCurrentRing()
{
	bRinging = false;
	RingAudio->Stop();
}

bool AHotelPhoneActor::CanInteract_Implementation(
	AActor* Interactor) const
{
	return !bEndingPlay && bRinging && !bInCall;
}

FText AHotelPhoneActor::GetInteractPrompt_Implementation(
	AActor* Interactor) const
{
	return AnswerPrompt;
}

void AHotelPhoneActor::Interact_Implementation(
	AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn
		? Cast<APlayerController>(Pawn->GetController())
		: nullptr;

	if (bEndingPlay || !bRinging || bInCall || !PC)
	{
		return;
	}

	// 벨이 울리던 중 요청자가 제거된 경우 해당 전화를 건너뜁니다.
	if (CurrentCall.bHasRequester
		&& !CurrentCall.Requester.IsValid())
	{
		StopRinging();
		return;
	}

	// 수신할 때는 다음 대기 전화가 시작되지 않도록
	// StopRinging() 대신 벨소리만 멈춥니다.
	StopCurrentRing();

	bInCall = true;
	CallerPC = PC;

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	PC->SetViewTargetWithBlend(
		this, CameraBlendTime, VTBlend_EaseInOut, 2.0f);

	SubtitleWidget = CreateWidget<UPhoneSubtitleWidget>(
		PC, UPhoneSubtitleWidget::StaticClass());

	if (SubtitleWidget)
	{
		SubtitleWidget->SetSubtitle(CurrentCall.Subtitle);
		SubtitleWidget->AddToViewport(UILayer::PhoneSubtitle);
	}

	GetWorldTimerManager().SetTimer(
		CallTimer,
		this,
		&ThisClass::EndCall,
		FMath::Max(0.1f, CallDuration),
		false);

	// 타이머 설정 후 알립니다.
	// 이벤트 처리 중 액터가 종료되더라도 EndPlay에서 정리됩니다.
	OnCallAnswered.Broadcast(CurrentCall.Requester.Get());
}

void AHotelPhoneActor::EndCall()
{
	if (!bInCall)
	{
		return;
	}

	// 완료 알림 처리 중 새 요청이 들어와도 순서를 유지합니다.
	bFinishingCall = true;
	bInCall = false;

	GetWorldTimerManager().ClearTimer(CallTimer);

	const TWeakObjectPtr<const UObject> FinishedRequester =
		CurrentCall.Requester;

	if (SubtitleWidget)
	{
		SubtitleWidget->RemoveFromParent();
		SubtitleWidget = nullptr;
	}

	APlayerController* PC = CallerPC.Get();
	CallerPC.Reset();

	if (PC)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(
				Pawn, CameraBlendTime, VTBlend_EaseInOut, 2.0f);
		}

		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}

	CurrentCall = FHotelPhoneCallRequest();

	// 게임 종료로 중단된 통화는 정상 완료로 알리지 않습니다.
	if (!bEndingPlay)
	{
		OnCallFinished.Broadcast(FinishedRequester.Get());
	}

	bFinishingCall = false;
	TryStartNextCall();
}

void AHotelPhoneActor::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	// 종료 중에는 다음 전화가 시작되지 않도록 먼저 설정합니다.
	bEndingPlay = true;

	PendingCalls.Reset();
	GetWorldTimerManager().ClearTimer(CallTimer);

	StopCurrentRing();
	EndCall();

	CurrentCall = FHotelPhoneCallRequest();

	Super::EndPlay(EndPlayReason);
}