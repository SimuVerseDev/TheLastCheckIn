#include "Scare/StoryRadio.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"

AStoryRadio::AStoryRadio()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	RadioAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioAudio"));
	RadioAudio->SetupAttachment(Mesh);
	RadioAudio->bAutoActivate = false;

	CounterZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CounterZone"));
	CounterZone->SetupAttachment(Mesh);
	CounterZone->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	CounterZone->SetCollisionProfileName(TEXT("Trigger"));

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(Mesh);
	ViewCamera->SetRelativeLocation(FVector(-60.f, 0.f, 25.f));
}