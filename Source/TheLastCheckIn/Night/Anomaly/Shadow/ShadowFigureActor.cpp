#include "Night/Anomaly/Shadow/ShadowFigureActor.h"
#include "Housekeeping/Objects/Vacuum/HKVacuumToolActor.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
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

	if (bCCTVOnly)
	{
		UpdateCCTVVisibility();
		SetActorTickEnabled(true);
	}
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
	return bPresent && !bAbsorbing && AHKVacuumToolActor::FindHeldBy(Interactor) != nullptr;
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
	const AHKVacuumToolActor* Vacuum = AHKVacuumToolActor::FindHeldBy(Interactor);
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
	if (bPresent && bCCTVOnly && !bAbsorbing)
	{
		UpdateCCTVVisibility();
	}
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

/*
CCTV는 플레이어 시점을 카메라 액터로 옮기는 방식 - 시점이 카메라일 때만 연기 표시
*/
void AShadowFigureActor::UpdateCCTVVisibility()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AActor* ViewTarget = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetViewTarget() : nullptr;
	const bool bOnCCTV = ViewTarget && ViewTarget->IsA<ACameraActor>();

	// 조준용 Mesh는 그대로 두고 연기 카드만 숨김
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim != Mesh)
		{
			Prim->SetHiddenInGame(!bOnCCTV);
		}
	}
}