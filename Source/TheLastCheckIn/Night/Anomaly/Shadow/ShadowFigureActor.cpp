#include "Night/Anomaly/Shadow/ShadowFigureActor.h"
#include "Housekeeping/Objects/Vacuum/HKVacuumToolActor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
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

	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (UMeshComponent* MeshComp : Meshes)
	{
		if (MeshComp != Mesh)
		{
			OriginalMaterials.Add(MeshComp, MeshComp->GetMaterials());
		}
	}
	Disappear();
}

void AShadowFigureActor::Appear()
{
	SetActorTransform(HomeTransform);
	RestoreMaterials();
	SetCardsHidden(false);
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
	AHKVacuumToolActor* Vacuum = AHKVacuumToolActor::FindHeldBy(Interactor);
	AbsorbVacuum = Vacuum;
	AbsorbStartLocation = GetActorLocation();
	AbsorbStartRotation = GetActorQuat();
	AbsorbStartScale = GetActorScale3D();
	AbsorbElapsed = 0.0f;
	bAbsorbing = true;
	SetActorEnableCollision(false);
	SetActorTickEnabled(true);

	Vacuum->BeginSuck(AbsorbDuration);
	return true;
}

void AShadowFigureActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bAbsorbing)
	{
		TickAbsorb(DeltaSeconds);
	}
	else if (bPresent && bCCTVOnly)
	{
		UpdateCCTVVisibility();
	}
}

/*
흡입 연출
- 버팀: 제자리에서 덜덜 떨며 흡입구 쪽으로 조금씩 기울고 늘어남
- 끌려감: 갑자기 가속하며 흡입구 방향으로 길게 늘어난 채 빨려 들어가고, 지직거리며 깜빡임
*/
void AShadowFigureActor::TickAbsorb(float DeltaSeconds)
{
	AbsorbElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(AbsorbElapsed / AbsorbDuration, 0.0f, 1.0f);

	const AHKVacuumToolActor* Vacuum = AbsorbVacuum.Get();
	const FVector Target = Vacuum ? Vacuum->GetSuckTargetLocation() : AbsorbStartLocation;
	const FVector ToTarget = Target - AbsorbStartLocation;
	const FVector Dir = ToTarget.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	// 형체의 위쪽(Z)이 흡입구를 향하도록 - 그 축으로 늘어나면 흡입구로 빨려가는 줄기처럼 보임
	const FQuat PullRotation = FRotationMatrix::MakeFromZX(Dir, AbsorbStartRotation.GetForwardVector()).ToQuat();

	const float Time = GetWorld()->GetTimeSeconds();
	const FVector Noise(
		FMath::PerlinNoise1D(Time * 35.0f),
		FMath::PerlinNoise1D(Time * 35.0f + 17.0f),
		FMath::PerlinNoise1D(Time * 35.0f + 41.0f));

	const float Resist = FMath::Max(ResistRatio, KINDA_SMALL_NUMBER);
	const float LeanDistance = FMath::Min(ToTarget.Size() * 0.08f, 12.0f);
	FVector Location;
	FQuat Rotation;
	FVector Scale;
	float Glitch;
	float FlickerChance;

	if (Alpha < Resist)
	{
		const float R = Alpha / Resist;
		Location = AbsorbStartLocation + Dir * LeanDistance * R + Noise * ResistShake * (0.3f + 0.7f * R);
		Rotation = FQuat::Slerp(AbsorbStartRotation, PullRotation, 0.2f * R);
		Scale = FVector(1.0f - 0.1f * R, 1.0f - 0.1f * R, 1.0f + 0.3f * R);
		Glitch = 0.3f + 0.5f * R;
		FlickerChance = 0.1f * R;
	}
	else
	{
		const float P = (Alpha - Resist) / (1.0f - Resist);
		const float Pull = P * P * P; // 처음엔 버티다 확 빨려 들어감
		const FVector From = AbsorbStartLocation + Dir * LeanDistance;
		Location = FMath::Lerp(From, Target, Pull) + Noise * ResistShake * 0.3f * (1.0f - P);
		Rotation = FQuat::Slerp(FQuat::Slerp(AbsorbStartRotation, PullRotation, 0.2f), PullRotation, FMath::Min(P * 4.0f, 1.0f));

		const float Stretch = FMath::Lerp(1.3f, StretchScale, FMath::Sin(FMath::Min(P * 1.4f, 1.0f) * UE_HALF_PI));
		const float Thin = FMath::Lerp(0.9f, 0.2f, P);
		const float Shrink = FMath::Lerp(1.0f, 0.05f, Pull);
		Scale = FVector(Thin, Thin, Stretch) * Shrink;
		Glitch = 1.0f;
		FlickerChance = 0.35f;
	}

	SetActorLocationAndRotation(Location, Rotation);
	SetActorScale3D(AbsorbStartScale * Scale);
	SetGlitch(Glitch);

	// CCTV 전용 형체는 CCTV로 볼 때만 그림
	const bool bBaseVisible = !bCCTVOnly || IsViewedOnCCTV();
	SetCardsHidden(!bBaseVisible || FMath::FRand() < FlickerChance);

	if (Alpha >= 1.0f)
	{
		Disappear();
		OnAbsorbed.Broadcast(this);
	}
}

void AShadowFigureActor::SetCardsHidden(bool bHideCards)
{
	// 조준용 Mesh는 그대로 두고 연기 카드만
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim != Mesh)
		{
			Prim->SetHiddenInGame(bHideCards);
		}
	}
}

void AShadowFigureActor::SetGlitch(float Value)
{
	if (GlitchParameter.IsNone())
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<UMeshComponent>, TArray<UMaterialInterface*>>& Pair : OriginalMaterials)
	{
		if (UMeshComponent* MeshComp = Pair.Key.Get())
		{
			MeshComp->SetScalarParameterValueOnMaterials(GlitchParameter, Value);
		}
	}
}

void AShadowFigureActor::RestoreMaterials()
{
	for (const TPair<TWeakObjectPtr<UMeshComponent>, TArray<UMaterialInterface*>>& Pair : OriginalMaterials)
	{
		UMeshComponent* MeshComp = Pair.Key.Get();
		if (!MeshComp)
		{
			continue;
		}
		for (int32 i = 0; i < Pair.Value.Num(); ++i)
		{
			MeshComp->SetMaterial(i, Pair.Value[i]);
		}
	}
}

/*
CCTV는 플레이어 시점을 카메라 액터로 옮기는 방식 - 시점이 카메라일 때만 연기 표시
*/
bool AShadowFigureActor::IsViewedOnCCTV() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AActor* ViewTarget = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetViewTarget() : nullptr;
	return ViewTarget && ViewTarget->IsA<ACameraActor>();
}

void AShadowFigureActor::UpdateCCTVVisibility()
{
	SetCardsHidden(!IsViewedOnCCTV());
}
