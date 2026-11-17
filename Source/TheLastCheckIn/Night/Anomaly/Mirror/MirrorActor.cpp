#include "Night/Anomaly/Mirror/MirrorActor.h"
#include "Night/Anomaly/Mirror/MirrorReflectionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AMirrorActor::AMirrorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(GetRootComponent());

	GlassMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlassMesh"));
	GlassMesh->SetupAttachment(GetRootComponent());
	GlassMesh->SetCollisionProfileName(TEXT("NoCollision"));
	GlassMesh->SetCastShadow(false);

	MirrorPlane = CreateDefaultSubobject<USceneComponent>(TEXT("MirrorPlane"));
	MirrorPlane->SetupAttachment(GlassMesh);

	Reflection = CreateDefaultSubobject<UMirrorReflectionComponent>(TEXT("Reflection"));
	Reflection->SetupAttachment(GetRootComponent());
}

void AMirrorActor::BeginPlay()
{
	// 반사 컴포넌트 BeginPlay에서 유리 머티리얼을 만들 수 있게 먼저 연결
	Reflection->SetupMirror(MirrorPlane, GlassMesh);
	Super::BeginPlay();
	SetCrackAmount(0.0f);
}

bool AMirrorActor::Crack()
{
	if (MirrorState != EMirrorState::Intact)
	{
		return false;
	}
	SetCrackAmount(1.0f);
	SetMirrorState(EMirrorState::Cracked);
	OnCracked();
	return true;
}

bool AMirrorActor::Shatter()
{
	if (MirrorState == EMirrorState::Shattered)
	{
		return false;
	}
	GlassMesh->SetVisibility(false);
	Reflection->SetReflectionEnabled(false);
	SetMirrorState(EMirrorState::Shattered);
	OnShattered();
	return true;
}

bool AMirrorActor::Repair()
{
	if (MirrorState == EMirrorState::Intact)
	{
		return false;
	}
	GlassMesh->SetVisibility(true);
	Reflection->SetReflectionEnabled(true);
	SetCrackAmount(0.0f);
	SetMirrorState(EMirrorState::Intact);
	OnRepaired();
	return true;
}

void AMirrorActor::SetMirrorState(EMirrorState NewState)
{
	const EMirrorState OldState = MirrorState;
	MirrorState = NewState;
	OnMirrorStateChanged.Broadcast(this, OldState, NewState);
}

void AMirrorActor::SetCrackAmount(float Amount) const
{
	if (UMaterialInstanceDynamic* MID = Reflection->GetSurfaceMaterial())
	{
		MID->SetScalarParameterValue(CrackParameterName, Amount);
	}
}
