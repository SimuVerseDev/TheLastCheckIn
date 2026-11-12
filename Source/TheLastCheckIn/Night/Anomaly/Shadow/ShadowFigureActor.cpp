#include "Night/Anomaly/Shadow/ShadowFigureActor.h"
#include "Night/Anomaly/Shadow/VacuumToolActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AShadowFigureActor::AShadowFigureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);

	/*
	플레이어 이동은 막지 않고, 시선 트레이스에만 Overlap으로 잡힘 - 얼룩 AimBox와 같은 방식
	*/
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);

	// TODO : BP에서 교체예정
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 1.2f));
	}
	AbsorbPrompt = NSLOCTEXT("Anomaly", "ShadowAbsorb", "흡입");
}

void AShadowFigureActor::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();
	Disappear();
}

void AShadowFigureActor::Appear()
{
	SetActorTransform(HomeTransform);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bPresent = true;
	bAbsorbing = false;
}

void AShadowFigureActor::Disappear()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	bPresent = false;
	bAbsorbing = false;
}

bool AShadowFigureActor::CanInteract_Implementation(AActor* Interactor) const
{
	return false;
}

bool AShadowFigureActor::CanUse_Implementation(AActor* Interactor) const
{
	return bPresent && !bAbsorbing && AVacuumToolActor::FindHeldBy(Interactor) != nullptr;
}

FText AShadowFigureActor::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return AbsorbPrompt;
}

bool AShadowFigureActor::Use_Implementation(AActor* Interactor)
{
	if (!CanUse_Implementation(Interactor))
	{
		return false;
	}
	const AVacuumToolActor* Vacuum = AVacuumToolActor::FindHeldBy(Interactor);
	AbsorbTarget = Vacuum->GetNozzle();
	AbsorbStartLocation = GetActorLocation();
	AbsorbStartScale = GetActorScale3D();
	AbsorbElapsed = 0.0f;
	bAbsorbing = true;
	SetActorEnableCollision(false);
	SetActorTickEnabled(true);
	return true;
}

void AShadowFigureActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAbsorbing)
	{
		return;
	}
	AbsorbElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(AbsorbElapsed / AbsorbDuration, 0.0f, 1.0f);
	const FVector Target = AbsorbTarget.IsValid() ? AbsorbTarget->GetComponentLocation() : AbsorbStartLocation;
	SetActorLocation(FMath::Lerp(AbsorbStartLocation, Target, Alpha));
	SetActorScale3D(FMath::Lerp(AbsorbStartScale, AbsorbStartScale * 0.05f, Alpha));

	if (Alpha >= 1.0f)
	{
		Disappear();
		OnAbsorbed.Broadcast(this);
	}
}