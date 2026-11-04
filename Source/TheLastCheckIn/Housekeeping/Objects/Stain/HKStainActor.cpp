#include "Housekeeping/Objects/Stain/HKStainActor.h"
#include "Housekeeping/Objects/Stain/HKStainDirtComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AHKStainActor::AHKStainActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(Root);
	Decal->DecalSize = FVector(8.0f, 40.0f, 40.0f);
	Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	
	AimBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AimBox"));
	AimBox->SetupAttachment(Decal);
	AimBox->SetBoxExtent(FVector(4.0f, 40.0f, 40.0f));
	AimBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AimBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	AimBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);

	Stain = CreateDefaultSubobject<UHKStainDirtComponent>(TEXT("Stain"));
}

FVector AHKStainActor::GetSurfaceNormal() const
{
	return -Decal->GetForwardVector();
}

void AHKStainActor::BeginPlay()
{
	Super::BeginPlay();
	DecalMaterial = Decal->CreateDynamicMaterialInstance();
	Stain->OnAmountChanged.AddDynamic(this, &AHKStainActor::HandleAmountChanged);
	HandleAmountChanged(Stain->GetAmount());
	
	if (bDirtyOnBeginPlay)
	{
		FRandomStream Random(FMath::Rand());
		Stain->ApplyDirt(Random);
	}
}

void AHKStainActor::HandleAmountChanged(float Amount)
{
	Decal->SetVisibility(Amount > 0.0f);
	AimBox->SetCollisionEnabled(Amount > 0.0f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (DecalMaterial)
	{
		DecalMaterial->SetScalarParameterValue(AmountParameter, Amount);
	}
}