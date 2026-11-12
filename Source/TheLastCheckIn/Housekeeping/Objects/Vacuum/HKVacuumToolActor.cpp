#include "Housekeeping/Objects/Vacuum/HKVacuumToolActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AHKVacuumToolActor::AHKVacuumToolActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	Nozzle = CreateDefaultSubobject<USceneComponent>(TEXT("Nozzle"));
	Nozzle->SetupAttachment(Mesh);

	PickupPrompt = NSLOCTEXT("Anomaly", "VacuumPickup", "청소기 들기");
	DropPrompt = NSLOCTEXT("Anomaly", "VacuumDrop", "청소기 내려놓기");
}

AHKVacuumToolActor* AHKVacuumToolActor::FindHeldBy(const AActor* Interactor)
{
	UWorld* World = Interactor ? Interactor->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHKVacuumToolActor> It(World); It; ++It)
	{
		if (It->Holder.Get() == Interactor)
		{
			return *It;
		}
	}
	return nullptr;
}

bool AHKVacuumToolActor::CanInteract_Implementation(AActor* Interactor) const
{
	return !Holder.IsValid() || Holder.Get() == Interactor;
}

FText AHKVacuumToolActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return Holder.Get() == Interactor ? DropPrompt : PickupPrompt;
}

void AHKVacuumToolActor::Interact_Implementation(AActor* Interactor)
{
	if (Holder.Get() == Interactor)
	{
		Unequip();
	}
	else if (!Holder.IsValid())
	{
		Equip(Interactor);
	}
}

bool AHKVacuumToolActor::IsInteractionActive_Implementation(AActor* Interactor) const
{
	return Holder.Get() == Interactor;
}

void AHKVacuumToolActor::Equip(AActor* Interactor)
{
	UCameraComponent* Camera = Interactor ? Interactor->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return;
	}
	Holder = Interactor;
	Mesh->SetSimulatePhysics(false);
	SetActorEnableCollision(false);
	AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeTransform(HeldOffset);
}

void AHKVacuumToolActor::Unequip()
{
	AActor* OldHolder = Holder.Get();
	Holder.Reset();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	/*
	들고 있던 위치 바로 아래 바닥에 세워 둠
	*/
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VacuumDrop), false, this);
	if (OldHolder)
	{
		Params.AddIgnoredActor(OldHolder);
	}
	const FVector Start = GetActorLocation();
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0f, 0.0f, 500.0f), ECC_Visibility, Params))
	{
		SetActorLocation(Hit.ImpactPoint);
	}
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	SetActorEnableCollision(true);
}