#include "Night/Anomaly/Fall/FallObjectActor.h"
#include "TheLastCheckIn.h"
#include "Trash/TrashBagActor.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Night/Anomaly/Mirror/MirrorReflectionComponent.h"
#include "Components/LightComponent.h"

// 유니티 빌드에서 다른 cpp와 이름이 겹치지 않도록 분리
namespace FallObjectMirror
{
	// 거울면 기준점 - 이 이름이거나 이 태그가 붙은 SceneComponent, 앞(X)이 거울이 바라보는 방향
	static const FName PlaneName(TEXT("MirrorPlane"));

	// 잔해와 함께 치워지고 복구되는 부속 메시 태그 (거울 틀 등)
	static const FName PartTag(TEXT("FallPart"));

	// 부속 메시의 빈 자리 표시용 복제본 (BeginPlay에서 생성)
	static const FName PartHintTag(TEXT("FallPartHint"));

	// 멀쩡할 때만 켜지는 라이트 태그 (스탠드 조명 등)
	static const FName LightTag(TEXT("FallLight"));
}
#include "Engine/World.h"
#include "TimerManager.h"

AFallObjectActor::AFallObjectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	/*
	BP_HK_TableLamp(HKPhysicsPickupActor)와 같은 물리 설정 - 시뮬레이션 켜기/끄기는 BeginPlay에서 bSimulateWhileIntact에 따라
	*/
	IntactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IntactMesh"));
	IntactMesh->SetupAttachment(Root);
	IntactMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	IntactMesh->SetMassOverrideInKg(NAME_None, IntactMassKg, true);
	IntactMesh->BodyInstance.bStartAwake = false;

	// 떨어지는 동안 물리로 쓰러졌다가 SettleTime 뒤 그 자리에 고정
	BrokenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrokenMesh"));
	BrokenMesh->SetupAttachment(Root);
	BrokenMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	BrokenMesh->SetMassOverrideInKg(NAME_None, IntactMassKg, true);

	// 빈 자리 표시 - 평소엔 숨김, 충돌·그림자 없음
	SlotHintMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlotHintMesh"));
	SlotHintMesh->SetupAttachment(Root);
	SlotHintMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SlotHintMesh->SetCastShadow(false);
	SlotHintMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HintMaterial(
		TEXT("/Game/HouseKeeping/Materials/M_HK_TrashHighlight.M_HK_TrashHighlight"));
	if (HintMaterial.Succeeded())
	{
		SlotHintMaterial = HintMaterial.Object;
	}

	CleanupPrompt = NSLOCTEXT("Anomaly", "FallCleanup", "꾹 눌러 잔해 봉투에 담기");
	PickupPrompt = NSLOCTEXT("Anomaly", "FallPickup", "잡기");
}

void AFallObjectActor::BeginPlay()
{
	Super::BeginPlay();
	IntactMesh->SetMassOverrideInKg(NAME_None, IntactMassKg, true);
	BrokenMesh->SetMassOverrideInKg(NAME_None, IntactMassKg, true);
	IntactHome = IntactMesh->GetRelativeTransform();
	BrokenHome = BrokenMesh->GetRelativeTransform();

	// 원래 자리에 원래 메시 모양으로 표시
	SlotHintMesh->SetStaticMesh(IntactMesh->GetStaticMesh());
	SlotHintMesh->SetRelativeTransform(IntactHome);
	if (SlotHintMaterial)
	{
		for (int32 i = 0; i < SlotHintMesh->GetNumMaterials(); ++i)
		{
			SlotHintMesh->SetMaterial(i, SlotHintMaterial);
		}
	}

	/*
	부속 메시(FallPart)도 빈 자리 표시에 같이 보이도록 같은 모양의 표시 메시를 런타임에 만듦
	*/
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents(MeshComponents);
	for (UStaticMeshComponent* Part : MeshComponents)
	{
		if (!Part->ComponentHasTag(FallObjectMirror::PartTag) || !Part->GetStaticMesh())
		{
			continue;
		}
		UStaticMeshComponent* PartHint = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		PartHint->SetupAttachment(Root);
		PartHint->SetMobility(EComponentMobility::Movable);
		PartHint->SetStaticMesh(Part->GetStaticMesh());
		PartHint->SetRelativeTransform(Part->GetComponentTransform().GetRelativeTransform(Root->GetComponentTransform()));
		PartHint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartHint->SetCastShadow(false);
		PartHint->SetVisibility(false);
		PartHint->ComponentTags.Add(FallObjectMirror::PartHintTag);
		if (SlotHintMaterial)
		{
			for (int32 i = 0; i < PartHint->GetNumMaterials(); ++i)
			{
				PartHint->SetMaterial(i, SlotHintMaterial);
			}
		}
		AddInstanceComponent(PartHint);
		PartHint->RegisterComponent();
	}

	ResetIntactMesh();
	ApplyState();
}

/*
빈 자리 표시 (SlotHintMesh + 부속 메시 표시) 보이기/숨기기
*/
static void SetFallSlotHintsVisible(AActor* Owner, UStaticMeshComponent* SlotHintMesh, bool bVisible)
{
	SlotHintMesh->SetVisibility(bVisible);
	TArray<UStaticMeshComponent*> MeshComponents;
	Owner->GetComponents(MeshComponents);
	for (UStaticMeshComponent* Component : MeshComponents)
	{
		if (Component->ComponentHasTag(FallObjectMirror::PartHintTag))
		{
			Component->SetVisibility(bVisible);
		}
	}
}

/*
즉시 잔해로 전환하고 잔해에 물리 -> 윗부분을 밀어서 쓰러뜨림 -> SettleTime 뒤 멈춘 자리에 고정
*/
bool AFallObjectActor::Fall()
{
	if (State != EFallObjectState::Intact || bFalling)
	{
		return false;
	}
	bFalling = true;
	bFellTonight = true;

	// 제자리 스냅 중이면 중단, 놓기 알림은 끊고 (놓는 순간 스냅되지 않도록) 들고 있으면 먼저 놓게 함
	StopIntactSnap();
	if (UHKCarryComponent* Carry = BoundCarry.Get())
	{
		Carry->OnReleased.RemoveDynamic(this, &ThisClass::HandleCarryReleased);
	}
	BoundCarry.Reset();
	ReleaseFromCarrier();

	// 플레이어가 멀쩡한 오브젝트를 밀거나 옮겨 놨으면 잔해도 그만큼 옮긴 자리에서 시작
	const FVector IntactHomeWorld = Root->GetComponentTransform().TransformPosition(IntactHome.GetLocation());
	const FVector Moved = IntactMesh->GetComponentLocation() - IntactHomeWorld;
	ResetBrokenMesh();
	BrokenMesh->AddWorldOffset(FVector(Moved.X, Moved.Y, 0.0f));

	IntactMesh->SetSimulatePhysics(false);
	State = EFallObjectState::Broken;
	ApplyState();

	// 제자리에서 깨지는 것은 물리 없이 BP에 배치한 자리 그대로 - 바로 고정
	if (!bFallOnBreak)
	{
		ResetBrokenMesh();
		OnFell();
		Settle();
		UE_LOG(LogAnomaly, Log, TEXT("[Fall] '%s' 제자리에서 깨짐"), *GetName());
		return true;
	}

	BrokenMesh->SetSimulatePhysics(true);
	BrokenMesh->WakeAllRigidBodies();

	// 세워 둔 것은 무작위 수평 방향, 벽에 걸린 것은 정면(벽 반대쪽)
	FVector Direction = GetActorForwardVector();
	if (bSimulateWhileIntact)
	{
		const float Yaw = FMath::FRandRange(0.0f, 360.0f);
		Direction = FRotator(0.0f, Yaw, 0.0f).Vector();
	}
	const FBoxSphereBounds Bounds = BrokenMesh->Bounds;
	const FVector Top = Bounds.Origin + FVector(0.0f, 0.0f, Bounds.BoxExtent.Z);
	BrokenMesh->AddImpulseAtLocation(Direction * FallImpulse * BrokenMesh->GetMass(), Top);

	GetWorldTimerManager().SetTimer(SettleTimer, this, &ThisClass::Settle, FMath::Max(SettleTime, KINDA_SMALL_NUMBER));
	OnFell();
	UE_LOG(LogAnomaly, Log, TEXT("[Fall] '%s' 낙하"), *GetName());
	return true;
}

bool AFallObjectActor::Restore()
{
	if (State != EFallObjectState::Empty)
	{
		return false;
	}
	State = EFallObjectState::Intact;
	StopIntactSnap();
	ResetIntactMesh();
	ResetBrokenMesh();
	ApplyState();
	OnRestored();
	return true;
}

void AFallObjectActor::SetSlotHintVisible(bool bVisible)
{
	if (bSlotHintRequested == bVisible)
	{
		return;
	}
	bSlotHintRequested = bVisible;
	SetFallSlotHintsVisible(this, SlotHintMesh, bSlotHintRequested && State == EFallObjectState::Empty);
}

/*
IInteractable
*/
bool AFallObjectActor::CanInteract_Implementation(AActor* Interactor) const
{
	if (ATrashBagActor* HeldBag = GetBagHeldBy(Interactor))
	{
		return IInteractable::Execute_CanInteract(HeldBag, Interactor);
	}
	// 쓰러지는 중에는 못 치움 - 멈춰서 고정된 뒤부터
	return State == EFallObjectState::Broken && !bFalling && ATrashBagActor::CanHolderTakeBag(Interactor);
}

FText AFallObjectActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	if (ATrashBagActor* HeldBag = GetBagHeldBy(Interactor))
	{
		return IInteractable::Execute_GetInteractPrompt(HeldBag, Interactor);
	}
	return CleanupPrompt;
}

float AFallObjectActor::GetInteractHoldDuration_Implementation(AActor* Interactor) const
{
	if (ATrashBagActor* HeldBag = GetBagHeldBy(Interactor))
	{
		return IInteractable::Execute_GetInteractHoldDuration(HeldBag, Interactor);
	}
	return CleanupHoldSeconds;
}

void AFallObjectActor::Interact_Implementation(AActor* Interactor)
{
	if (ATrashBagActor* HeldBag = GetBagHeldBy(Interactor))
	{
		IInteractable::Execute_Interact(HeldBag, Interactor);
		return;
	}
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}
	if (!BagClass)
	{
		UE_LOG(LogAnomaly, Warning, TEXT("[Fall] '%s' BagClass가 설정되지 않음"), *GetName());
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATrashBagActor* NewBag = GetWorld()->SpawnActor<ATrashBagActor>(BagClass, BrokenMesh->GetComponentTransform(), Params);
	if (!NewBag)
	{
		return;
	}
	if (!NewBag->TryEquip(Interactor))
	{
		UE_LOG(LogAnomaly, Warning, TEXT("[Fall] '%s' 봉투를 손에 들 수 없음 (메시/소켓 확인)"), *GetName());
		NewBag->Destroy();
		return;
	}

	Bag = NewBag;
	NewBag->OnBagDisposed.AddUniqueDynamic(this, &ThisClass::HandleBagDisposed);
	// 봉투에 담았으면 평판 감소 대상 아님 (봉투를 버리지 않고 바닥에 둬도 감소 없음)
	bFellTonight = false;
	State = EFallObjectState::Bagged;
	ApplyState();
	OnCleanedUp();
}

/*
방금 담은 봉투를 들고 있는 동안에는 이 액터가 진행 중인 상호작용으로 남아서 E를 봉투로 전달
봉투를 내려놓으면 false가 되고, 이후 바닥의 봉투는 봉투 자신이 상호작용 대상
*/
bool AFallObjectActor::IsInteractionActive_Implementation(AActor* Interactor) const
{
	return GetBagHeldBy(Interactor) != nullptr;
}

/*
LMB 들기
*/
bool AFallObjectActor::CanUse_Implementation(AActor* Interactor) const
{
	const UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	return Carry && !Carry->IsCarrying() && CanBeCarried();
}

FText AFallObjectActor::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return PickupPrompt;
}

bool AFallObjectActor::Use_Implementation(AActor* Interactor)
{
	UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	UStaticMeshComponent* Target = GetCarriableMesh();
	if (!Carry || !Target || Carry->IsCarrying())
	{
		return false;
	}

	/*
	AHKPhysicsPickupActor::Use와 같은 흐름 - 물리를 켜고 잡음, 실패하면 되돌림
	bUprightWhileCarried: HKCarry는 AHKPhysicsPickupActor만 세워서 들기 때문에 잡기 직전에 똑바로 세워 같은 결과를 냄
	*/
	const bool bIntact = Target == IntactMesh;
	const bool bWasSimulating = Target->IsSimulatingPhysics();
	const FRotator WasRotation = Target->GetComponentRotation();
	if (bIntact && bUprightWhileCarried)
	{
		Target->SetWorldRotation(FRotator(0.0f, WasRotation.Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	Target->SetSimulatePhysics(true);
	if (!Carry->TryPickup(Target))
	{
		Target->SetSimulatePhysics(bWasSimulating);
		Target->SetWorldRotation(WasRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return false;
	}

	if (bIntact)
	{
		Carry->OnReleased.AddUniqueDynamic(this, &ThisClass::HandleCarryReleased);
		BoundCarry = Carry;
	}
	return true;
}

/*
들고 있던 멀쩡한 오브젝트를 놓음 - BP_HK_TableLamp처럼 던지지 않았으면 원래 자리 근처일 때 제자리 스냅
*/
void AFallObjectActor::HandleCarryReleased(UPrimitiveComponent* Component, bool bThrown)
{
	if (Component != IntactMesh)
	{
		return;
	}
	if (UHKCarryComponent* Carry = BoundCarry.Get())
	{
		Carry->OnReleased.RemoveDynamic(this, &ThisClass::HandleCarryReleased);
	}
	BoundCarry.Reset();

	if (!bThrown && State == EFallObjectState::Intact)
	{
		TrySnapIntactHome();
	}
}

/*
UHKPlacementDirtComponent::TrySnapHome과 같은 판정 - 수평 거리, 높이 차, 기울기
*/
bool AFallObjectActor::TrySnapIntactHome()
{
	if (IntactSnapTimer.IsValid())
	{
		return false;
	}

	const FTransform Home = IntactHome * Root->GetComponentTransform();
	const FTransform Current = IntactMesh->GetComponentTransform();
	const FVector Delta = Current.GetLocation() - Home.GetLocation();
	const float Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Current.GetRotation().GetUpVector(), Home.GetRotation().GetUpVector()), -1.0f, 1.0f)));
	if (Delta.Size2D() > SnapDistance || FMath::Abs(Delta.Z) > SnapHeight || Tilt > SnapAngle)
	{
		return false;
	}

	// 이동 중엔 물리 끄기 - 끝나면 제자리에 고정 (물리 꺼진 채)
	IntactMesh->SetSimulatePhysics(false);
	IntactSnapStart = Current;
	IntactSnapStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(IntactSnapTimer, this, &ThisClass::TickIntactSnap, 0.01f, true);
	return true;
}

void AFallObjectActor::TickIntactSnap()
{
	const FTransform Home = IntactHome * Root->GetComponentTransform();
	const float Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - IntactSnapStartTime) / FMath::Max(SnapDuration, 0.01f), 0.0f, 1.0f);
	const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

	IntactMesh->SetWorldLocationAndRotation(
		FMath::Lerp(IntactSnapStart.GetLocation(), Home.GetLocation(), Eased),
		FQuat::Slerp(IntactSnapStart.GetRotation(), Home.GetRotation(), Eased));

	if (Alpha >= 1.0f)
	{
		StopIntactSnap();
		ResetIntactMesh();
	}
}

void AFallObjectActor::StopIntactSnap()
{
	GetWorldTimerManager().ClearTimer(IntactSnapTimer);
}

/*
private
*/
void AFallObjectActor::HandleBagDisposed(ATrashBagActor* DisposedBag)
{
	if (Bag.Get() != DisposedBag)
	{
		return;
	}
	Bag.Reset();
	State = EFallObjectState::Empty;
	ApplyState();
	OnCleared();
}

ATrashBagActor* AFallObjectActor::GetBagHeldBy(const AActor* Interactor) const
{
	ATrashBagActor* HeldBag = Bag.Get();
	return HeldBag && HeldBag->IsHeldBy(Interactor) ? HeldBag : nullptr;
}

/*
쓰러지는 시간이 끝남 - 물리는 유지하고 잠재움 (멀쩡할 때처럼 밀리고 LMB로 들 수 있음), 원래 자리로 돌아가지 않음
*/
void AFallObjectActor::Settle()
{
	bFalling = false;
	BrokenMesh->PutAllRigidBodiesToSleep();
	OnBroken();
}

/*
물리로 떨어져 나간 BrokenMesh를 Root에 다시 붙이고 BP에 배치한 자리로 되돌림
*/
void AFallObjectActor::ResetBrokenMesh()
{
	BrokenMesh->SetSimulatePhysics(false);
	BrokenMesh->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	BrokenMesh->SetRelativeTransform(BrokenHome);
}

/*
물리로 떨어져 나간 IntactMesh를 Root에 다시 붙이고 원래 자리로 되돌림
*/
void AFallObjectActor::ResetIntactMesh()
{
	// BP_HK_TableLamp(HKPlacementDirtComponent)처럼 제자리에서는 물리를 끈 채 고정 - 잡는 순간 Use에서 켬
	IntactMesh->SetSimulatePhysics(false);
	IntactMesh->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	IntactMesh->SetRelativeTransform(IntactHome);
}

/*
지금 LMB로 들 수 있는 메시 - 멀쩡하면 IntactMesh, 잔해(쓰러지는 중 제외)면 BrokenMesh
*/
UStaticMeshComponent* AFallObjectActor::GetCarriableMesh() const
{
	if (!bCanBePickedUp || bFalling)
	{
		return nullptr;
	}
	if (State == EFallObjectState::Intact)
	{
		// 고정돼 있어도(물리 꺼짐) 들 수 있음 - 제자리 스냅 중에는 불가
		return bSimulateWhileIntact && !IntactSnapTimer.IsValid() ? IntactMesh.Get() : nullptr;
	}
	if (State == EFallObjectState::Broken)
	{
		return BrokenMesh->IsSimulatingPhysics() ? BrokenMesh.Get() : nullptr;
	}
	return nullptr;
}

bool AFallObjectActor::CanBeCarried() const
{
	return GetCarriableMesh() != nullptr;
}

/*
플레이어 Carry가 이 오브젝트의 메시를 들고 있으면 놓게 함
*/
void AFallObjectActor::ReleaseFromCarrier()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UHKCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UHKCarryComponent>() : nullptr;
		const UPrimitiveComponent* Carried = Carry ? Carry->GetCarriedComponent() : nullptr;
		if (Carried && (Carried == IntactMesh || Carried == BrokenMesh))
		{
			Carry->Drop();
		}
	}
}

void AFallObjectActor::ApplyState()
{
	const bool bIntact = State == EFallObjectState::Intact;
	const bool bBroken = State == EFallObjectState::Broken;

	IntactMesh->SetVisibility(bIntact);
	IntactMesh->SetCollisionEnabled(bIntact ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// 숨겨진 잔해가 콜리전 없이 물리로 바닥 아래로 떨어지지 않도록
	if (!bBroken)
	{
		BrokenMesh->SetSimulatePhysics(false);
	}
	BrokenMesh->SetVisibility(bBroken);
	BrokenMesh->SetCollisionEnabled(bBroken ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	SetFallSlotHintsVisible(this, SlotHintMesh, bSlotHintRequested && State == EFallObjectState::Empty);

	/*
	부속 메시 (거울 틀 등) - "FallPart" 태그가 붙은 컴포넌트는 멀쩡하거나 깨져 있는 동안만 보이고
	잔해를 봉투에 담으면 같이 사라졌다가 복구되면 다시 나타남
	*/
	{
		const bool bPartsPresent = bIntact || bBroken;
		TArray<UPrimitiveComponent*> Primitives;
		GetComponents(Primitives);
		for (UPrimitiveComponent* Part : Primitives)
		{
			if (Part->ComponentHasTag(FallObjectMirror::PartTag))
			{
				Part->SetVisibility(bPartsPresent, true);
				Part->SetCollisionEnabled(bPartsPresent ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			}
		}
	}

	/*
	라이트 (스탠드 조명 등) - IntactMesh에 붙어 있거나 "FallLight" 태그가 붙은 라이트는 멀쩡할 때만 켜지고 떨어지면 꺼짐
	배치된 인스턴스에 태그가 안 넘어오는 경우가 있어 부착 관계로도 판단
	*/
	{
		TArray<ULightComponent*> Lights;
		GetComponents(Lights);
		for (ULightComponent* Light : Lights)
		{
			if (Light->IsAttachedTo(IntactMesh) || Light->ComponentHasTag(FallObjectMirror::LightTag))
			{
				Light->SetVisibility(bIntact);
			}
		}
	}

	/*
	거울 반사 - BP에 MirrorReflectionComponent와 "MirrorPlane" SceneComponent가 있을 때만
	멀쩡하면 IntactMesh, 깨지면 BrokenMesh에 반사를 넘기고, 치운 뒤엔 끔
	*/
	if (UMirrorReflectionComponent* Reflection = FindComponentByClass<UMirrorReflectionComponent>())
	{
		USceneComponent* Plane = nullptr;
		TArray<USceneComponent*> SceneComponents;
		GetComponents(SceneComponents);
		for (USceneComponent* Component : SceneComponents)
		{
			if (Component->GetFName() == FallObjectMirror::PlaneName || Component->ComponentHasTag(FallObjectMirror::PlaneName))
			{
				Plane = Component;
				break;
			}
		}

		UStaticMeshComponent* Surface = bIntact ? IntactMesh.Get() : (bBroken ? BrokenMesh.Get() : nullptr);
		if (Plane && Surface)
		{
			Reflection->SetupMirror(Plane, Surface);
			Reflection->SetReflectionEnabled(true);
		}
		else
		{
			Reflection->SetReflectionEnabled(false);
		}
	}
}
