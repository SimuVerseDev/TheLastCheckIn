#include "Scare/StoryRadio.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "GameFramework/Pawn.h"

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
	CounterZone->OnComponentBeginOverlap.AddDynamic(this, &AStoryRadio::HandleCounterBeginOverlap);
	
	WindowStartSeconds = GameTime::GetDayStartSeconds(BroadcastDay - 1) + GameTime::HoursToSeconds(StartHour);
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
	if (State == EStoryRadioState::Static && CurrentSeconds >= StaticEndSeconds)
	{
		StopRadioAudio(StaticFadeOutSeconds);
		SetState(EStoryRadioState::Done);
		return;
	}
	
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
		
		if (IsPlayerInCounter())
		{
			StartStatic();
		}
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

void AStoryRadio::HandleCounterBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 최초 진입만: Waiting일 때 딱 한 번
	if (State == EStoryRadioState::Waiting && IsPlayerActor(OtherActor))
	{
		StartStatic();
	}
}

bool AStoryRadio::IsPlayerActor(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

bool AStoryRadio::IsPlayerInCounter() const
{
	TArray<AActor*> Overlapping;
	CounterZone->GetOverlappingActors(Overlapping, APawn::StaticClass());

	for (const AActor* Actor : Overlapping)
	{
		if (IsPlayerActor(Actor))
		{
			return true;
		}
	}
	return false;
}

void AStoryRadio::StartStatic()
{
	if (!StaticSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StoryRadio] StaticSound 미지정"));
		return;
	}

	StaticEndSeconds =
		GetNowSeconds() + GameTime::HoursToSeconds(StaticHours);

	RadioAudio->SetSound(StaticSound);
	RadioAudio->FadeIn(StaticFadeInSeconds);

	SetState(EStoryRadioState::Static);
}

void AStoryRadio::StopRadioAudio(float FadeOutSeconds)
{
	if (RadioAudio->IsPlaying())
	{
		RadioAudio->FadeOut(FadeOutSeconds, 0.f);
	}
}

double AStoryRadio::GetNowSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UGameClockSubsystem* Clock = World->GetSubsystem<UGameClockSubsystem>())
		{
			return Clock->GetTotalGameSeconds();
		}
	}
	return 0.0;
}