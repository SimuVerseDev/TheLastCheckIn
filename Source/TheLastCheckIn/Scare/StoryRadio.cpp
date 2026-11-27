#include "Scare/StoryRadio.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"

AStoryRadio::AStoryRadio()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	/*
	자동 재생이 꺼진 오디오 컴포넌트
	*/
	RadioAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioAudio"));
	RadioAudio->SetupAttachment(Mesh);
	RadioAudio->bAutoActivate = false;

	/*
	트리거 판정용 박스 영역
	*/
	CounterZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CounterZone"));
	CounterZone->SetupAttachment(Mesh);
	CounterZone->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	CounterZone->SetCollisionProfileName(TEXT("Trigger"));

	/*
	시점 카메라 배치
	*/
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(Mesh);
	ViewCamera->SetRelativeLocation(FVector(-60.f, 0.f, 25.f));
}

void AStoryRadio::BeginPlay()
{
	Super::BeginPlay();
	
	WindowStartSeconds = GameTime::GetDayStartSeconds(BroadcastDay - 1) + GameTime::GetDayStartSeconds(StartHour);
	WindowEndSeconds = WindowStartSeconds + GameTime::HoursToSeconds(WindowHours);
	
	if (UWorld* World = GetWorld())
	{
		if (UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnTimeAdvanced.AddDynamic(this, &AStoryRadio::HandleTimeAdvanced);
        		
			const double Now = Clock->GetTotalGameSeconds();
			HandleTimeAdvanced(Now, Now);
		}
	}
}

void AStoryRadio::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>())
		{
			Clock->OnTimeAdvanced.RemoveDynamic(
				this, &AStoryRadio::HandleTimeAdvanced);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AStoryRadio::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	if (CurrentSeconds >= WindowEndSeconds)
	{
		if (State == EStoryRadioState::Off || State == EStoryRadioState::Waiting)
		{
			SetState(EStoryRadioState::Done);
		}
		return;
	}
	
	if (State == EStoryRadioState::Off && CurrentSeconds >= WindowStartSeconds)
	{
		SetState(EStoryRadioState::Waiting);
	}
}

void AStoryRadio::SetState(EStoryRadioState NewState)
{
	if (State == NewState)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[StoryRadio] %s -> %s"),
		*UEnum::GetValueAsString(State),
		*UEnum::GetValueAsString(NewState));

	State = NewState;
}