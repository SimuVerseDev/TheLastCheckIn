#include "Night/Anomaly/Knock/KnockDoorActor.h"
#include "Night/Anomaly/Knock/HorseshoeActor.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Rooms/RoomActor.h"

AKnockDoorActor::AKnockDoorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	KnockAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("KnockAudio"));
	DropPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DropPoint"));
	DropPoint->SetupAttachment(GetRootComponent());
	
	Hinge->SetupAttachment(GetRootComponent());
	DoorMesh->SetupAttachment(Hinge);
	KnockAudio->SetupAttachment(GetRootComponent());
	KnockAudio->bAutoActivate = false;

	OpenPrompt = NSLOCTEXT("Knock", "Open", "문 열기");
	ClosePrompt = NSLOCTEXT("Knock", "Close", "문 닫기");
}

void AKnockDoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float TargetYaw = bOpen ? OpenYaw : 0.0f;
	CurrentYaw = FMath::FInterpConstantTo
	(
	CurrentYaw,
	TargetYaw,
	DeltaSeconds,
	OpenSpeed
	);
	Hinge->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f)); // 계산한 회전각(여기선 z축)을 실제 언리얼 컴포넌트에 적용

	if (FMath::IsNearlyEqual(CurrentYaw, TargetYaw))
	{
		SetActorTickEnabled(false); // 문이 이미 다 열렸거나 닫혔다면 회전계산 중단
	}
}

void AKnockDoorActor::SetOpen(bool bInOpen) // 문열기 : SetOpen(true) 호출 | 문닫기 : SetOpen(false) 호출
{
	if (!bCanOpen || bOpen == bInOpen)
	{
		return;
	}
	bOpen = bInOpen;
	SetActorTickEnabled(true); // Tick활성화
	OnOpenChanged.Broadcast(this, bOpen);
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


/*
Interactable
*/
bool AKnockDoorActor::CanInteract_Implementation(AActor* Interactor) const
{
	return bCanOpen;
}

FText AKnockDoorActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return bOpen ? ClosePrompt : OpenPrompt;
}

void AKnockDoorActor::Interact_Implementation(AActor* Interactor)
{
	SetOpen(!bOpen);
}