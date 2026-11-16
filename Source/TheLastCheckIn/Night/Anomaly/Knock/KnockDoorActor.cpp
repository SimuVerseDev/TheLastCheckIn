#include "Night/Anomaly/Knock/KnockDoorActor.h"
#include "Night/Anomaly/Knock/HorseshoeActor.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Rooms/RoomActor.h"

AKnockDoorActor::AKnockDoorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	KnockAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("KnockAudio"));
	DropPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DropPoint"));

	KnockAudio->SetupAttachment(GetRootComponent());
	KnockAudio->bAutoActivate = false;
	DropPoint->SetupAttachment(GetRootComponent());
}

bool AKnockDoorActor::IsOpen() const
{
	const AActor* Door = LinkedDoor.Get();
	if (!Door)
	{
		return false;
	}

	if (const FBoolProperty* Prop = FindFProperty<FBoolProperty>(Door->GetClass(), TEXT("bIsOpen")))
	{
		return Prop->GetPropertyValue_InContainer(Door);
	}
	return false;
}

void AKnockDoorActor::PlayKnock()
{
	if (KnockSound)
	{
		KnockAudio->SetSound(KnockSound);
		KnockAudio->Play();
	}
}

void AKnockDoorActor::StopKnock()
{
	KnockAudio->Stop();
}

bool AKnockDoorActor::TryPlaceHorseshoe(AHorseshoeActor* Horseshoe)
{
	if (!bAwaitingHorseshoe || !IsValid(Horseshoe) || Horseshoe->IsUsed())
	{
		return false;
	}
	const float DistSq = FVector::DistSquared(Horseshoe->GetActorLocation(), DropPoint->GetComponentLocation());
	if (DistSq > FMath::Square(DropRadius))
	{
		return false;
	}

	// fix
	Horseshoe->GetMesh()->SetSimulatePhysics(false);
	Horseshoe->SetActorLocationAndRotation(DropPoint->GetComponentLocation(), DropPoint->GetComponentRotation());
	Horseshoe->MarkUsed();

	bAwaitingHorseshoe = false;
	StopKnock();
	OnHorseshoePlaced.Broadcast(this);
	return true;
}
