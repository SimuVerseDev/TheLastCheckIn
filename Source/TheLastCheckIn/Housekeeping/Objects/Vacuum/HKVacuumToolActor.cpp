#include "Housekeeping/Objects/Vacuum/HKVacuumToolActor.h"
#include "Housekeeping/Objects/Vacuum/HKVacuumShakeModifier.h"
#include "TheLastCheckInCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AHKVacuumToolActor::AHKVacuumToolActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	HolderCamera = Camera;
	Mesh->SetSimulatePhysics(false);
	SetActorEnableCollision(false);

	/*
	1인칭 팔 소켓이 있으면 손에 쥐고, 없으면 예전처럼 카메라 앞에 고정
	놓여 있던 월드 위치에서 손까지 EquipBlendTime 동안 끌려오도록 현재 위치를 유지한 채 붙임
	*/
	const ATheLastCheckInCharacter* Character = Cast<ATheLastCheckInCharacter>(Interactor);
	USkeletalMeshComponent* Arms = Character ? Character->GetFirstPersonMesh() : nullptr;
	bInHand = Arms && !HandSocket.IsNone() && Arms->DoesSocketExist(HandSocket);
	if (bInHand)
	{
		AttachToComponent(Arms, FAttachmentTransformRules::KeepWorldTransform, HandSocket);
	}
	else
	{
		AttachToComponent(Camera, FAttachmentTransformRules::KeepWorldTransform);
	}
	EquipStart = GetRootComponent()->GetRelativeTransform();
	EquipElapsed = 0.0f;
	SuckElapsed = SuckDuration = 0.0f;
	Sway = FRotator::ZeroRotator;
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		LastControlRotation = Pawn->GetControlRotation();
	}

	SetFirstPersonRender(bInHand);
	SetActorTickEnabled(true);
}

void AHKVacuumToolActor::Unequip()
{
	AActor* OldHolder = Holder.Get();
	Holder.Reset();
	HolderCamera.Reset();
	SetActorTickEnabled(false);
	SetFirstPersonRender(false);
	bInHand = false;
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

/*
손에 든 동안은 팔과 같은 1인칭 FOV/스케일로 그려서 손과 어긋나지 않게 함
*/
void AHKVacuumToolActor::SetFirstPersonRender(bool bFirstPerson)
{
	TInlineComponentArray<UPrimitiveComponent*> Prims(this);
	for (UPrimitiveComponent* Prim : Prims)
	{
		Prim->FirstPersonPrimitiveType = bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None;
		Prim->MarkRenderStateDirty();
	}
}

FTransform AHKVacuumToolActor::GetHoldTransform() const
{
	return bInHand ? HandTransform : HeldOffset;
}

void AHKVacuumToolActor::BeginSuck(float Duration)
{
	SuckDuration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	SuckElapsed = 0.0f;

	const APawn* Pawn = Cast<APawn>(Holder.Get());
	UHKVacuumShakeModifier::Play(Pawn ? Pawn->GetController<APlayerController>() : nullptr, SuckDuration, CameraShakeStrength);
}

void AHKVacuumToolActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APawn* Pawn = Cast<APawn>(Holder.Get());
	if (!Pawn)
	{
		SetActorTickEnabled(false);
		return;
	}

	const FTransform Hold = GetHoldTransform();
	const FVector KeepScale = EquipStart.GetScale3D();

	/*
	시선을 돌리면 반대로 살짝 기울었다가 돌아옴
	*/
	const FRotator Control = Pawn->GetControlRotation();
	const FRotator Delta = (Control - LastControlRotation).GetNormalized();
	LastControlRotation = Control;
	const FRotator SwayTarget(
		FMath::Clamp(Delta.Pitch * 0.6f, -SwayAmount, SwayAmount),
		FMath::Clamp(-Delta.Yaw * 0.6f, -SwayAmount, SwayAmount),
		FMath::Clamp(-Delta.Yaw * 0.4f, -SwayAmount, SwayAmount));
	Sway = FMath::RInterpTo(Sway, SwayTarget, DeltaSeconds, 10.0f);

	FVector Location = Hold.GetLocation();
	FQuat Rotation = Hold.GetRotation() * Sway.Quaternion();

	/*
	흡입 중 - 뒤로 밀리는 반동과 잔떨림, 끝나갈수록 강해짐
	*/
	if (SuckElapsed < SuckDuration)
	{
		SuckElapsed += DeltaSeconds;
		const float Progress = FMath::Clamp(SuckElapsed / SuckDuration, 0.0f, 1.0f);
		const float Strength = 0.4f + 0.6f * Progress;
		const float T = GetWorld()->GetTimeSeconds() * 40.0f;
		Location += FVector(
			FMath::PerlinNoise1D(T) * SuckJitter * Strength - 1.5f * Strength,
			FMath::PerlinNoise1D(T + 13.3f) * SuckJitter * Strength,
			FMath::PerlinNoise1D(T + 27.9f) * SuckJitter * Strength);
		Rotation *= FRotator(0.0f, 0.0f, FMath::PerlinNoise1D(T + 51.2f) * 2.0f * Strength).Quaternion();
	}

	/*
	집어 든 직후에는 놓여 있던 자리에서 손으로 끌려옴
	*/
	if (EquipElapsed < EquipBlendTime)
	{
		EquipElapsed += DeltaSeconds;
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(EquipElapsed / EquipBlendTime, 0.0f, 1.0f), 3.0f);
		Location = FMath::Lerp(EquipStart.GetLocation(), Location, Alpha);
		Rotation = FQuat::Slerp(EquipStart.GetRotation(), Rotation, Alpha);
	}

	GetRootComponent()->SetRelativeTransform(FTransform(Rotation, Location, KeepScale));
}

FVector AHKVacuumToolActor::GetSuckTargetLocation() const
{
	const FVector NozzleLocation = Nozzle->GetComponentLocation();
	const UCameraComponent* Camera = HolderCamera.Get();
	if (!bInHand || !Camera)
	{
		return NozzleLocation;
	}

	/*
	1인칭 프리미티브는 카메라 쪽으로 FirstPersonScale만큼 당겨지고 FirstPersonFieldOfView로 투영됨
	같은 화면 위치에 보이는 월드 좌표로 되돌림
	*/
	const FTransform CameraTransform = Camera->GetComponentTransform();
	FVector Local = CameraTransform.InverseTransformPositionNoScale(NozzleLocation);
	if (Camera->bEnableFirstPersonScale)
	{
		Local *= Camera->FirstPersonScale;
	}
	if (Camera->bEnableFirstPersonFieldOfView)
	{
		const float WorldTan = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
		const float FirstPersonTan = FMath::Tan(FMath::DegreesToRadians(Camera->FirstPersonFieldOfView * 0.5f));
		if (FirstPersonTan > KINDA_SMALL_NUMBER)
		{
			const float Ratio = WorldTan / FirstPersonTan;
			Local.Y *= Ratio;
			Local.Z *= Ratio;
		}
	}
	return CameraTransform.TransformPositionNoScale(Local);
}
