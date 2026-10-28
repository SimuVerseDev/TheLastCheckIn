#include "Housekeeping/Core/Carry/HKCarryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Housekeeping/Objects/Blanket/HKBlanketActor.h"
#include "EngineUtils.h"

UHKCarryComponent::UHKCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

/*
virtual
*/
void UHKCarryComponent::BeginPlay()
{
	Super::BeginPlay();
	PhysicsHandle = NewObject<UPhysicsHandleComponent>(GetOwner(), TEXT("HKPhysicsHandle"));
	PhysicsHandle->RegisterComponent();
}

void UHKCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Drop();
	Super::EndPlay(EndPlayReason);
}

void UHKCarryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsCarrying())
	{
		return;
	}
	FVector HoldLocation;
	FRotator ViewRotation;
	if (!GetViewTransform(HoldLocation, ViewRotation))
	{
		Release(false);
		return;
	}
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		const FVector FrameVelocity = (HoldLocation - LastHoldLocation) / DeltaTime;
		HoldVelocity = FMath::VInterpTo(HoldVelocity, FrameVelocity, DeltaTime, 20.0f);
	}
	LastHoldLocation = HoldLocation;
	/*
	if picking blanket
	*/
	if (AHKBlanketActor* Blanket = CarriedBlanket.Get())
	{
		Blanket->SetGrabTarget(HoldLocation);
		return;
	}

	// A = CarriedComponent->GetComponentLocation(): 들고 있는 물체가 지금 실제로 있는 위치
	// B = HoldLocation: 물체가 있어야 할 위치
	// FVector::Dist(A, B)는 두 점 사이의 거리(cm)를 구하는 함수  
	if (!IsValid(CarriedComponent) || !CarriedComponent->IsSimulatingPhysics() || FVector::Dist(CarriedComponent->GetComponentLocation(), HoldLocation) > BreakDistance)
	{
		Release(false);
		return;
	}

	const FQuat YawQuat = FRotator(0.0f, ViewRotation.Yaw, 0.0f).Quaternion();
	PhysicsHandle->SetTargetLocationAndRotation(HoldLocation, (YawQuat * RotationOffset).Rotator());
}

bool UHKCarryComponent::TryPickup(UPrimitiveComponent* Target)
{
	// 기존 코드의 맨 처음 있던 IsSimulatingPhysics() 조건은 이불 때문에 제거하거나 아래로 내려야 합니다.
	if (IsCarrying() || !PhysicsHandle || !IsValid(Target))
	{
		return false;
	}
	
	FVector HoldLocation;
	FRotator ViewRotation;
	if (!GetViewTransform(HoldLocation, ViewRotation))
	{
		return false;
	}
	
	/*
	blanket
	*/
	if (AHKBlanketActor* Blanket = Cast<AHKBlanketActor>(Target->GetOwner()))
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const FVector RayStart = Pawn->GetPawnViewLocation();
		const FVector RayEnd = RayStart + ViewRotation.Vector() * 300.0f;
		const int32 ParticleIndex = Blanket->FindParticleNearRay(RayStart, RayEnd, 30.0f); // 이불을 이루는 점(파티클) 중 몇 번째 점
		if (ParticleIndex == INDEX_NONE)
		{
			return false;
		}

		Blanket->Grab(ParticleIndex);
		CarriedBlanket = Blanket;
		CarriedComponent = Target;
		LastHoldLocation = HoldLocation;
		HoldVelocity = FVector::ZeroVector;
		
		OnPickedUp.Broadcast(Target);
		return true;
	}
	
	// else
	if (!Target->IsSimulatingPhysics() || Target->GetMass() > MaxCarryMass)
	{
		return false;
	}
    
	CarriedComponent = Target;
	LastHoldLocation = HoldLocation;
	HoldVelocity = FVector::ZeroVector;
	const FQuat YawQuat = FRotator(0.0f, ViewRotation.Yaw, 0.0f).Quaternion();
	RotationOffset = YawQuat.Inverse() * Target->GetComponentQuat(); // * 기호 확인
    
	SavedPawnResponse = Target->GetCollisionResponseToChannel(ECC_Pawn);
	Target->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Target->WakeRigidBody();
	PhysicsHandle->GrabComponentAtLocationWithRotation(Target, NAME_None, Target->GetComponentLocation(), Target->GetComponentRotation());
	
	if (AHKPhysicsPickupActor* Pickup = Cast<AHKPhysicsPickupActor>(Target->GetOwner()))
	{
		Pickup->NotifyPickedUp(GetOwner());
	}
	OnPickedUp.Broadcast(Target);
	return true;
}

void UHKCarryComponent::Drop()
{
	if (IsCarrying())
	{
		Release(true);
	}
}

USceneComponent* UHKCarryComponent::GetTrashAbsorbTarget() const
{
	if (USceneComponent* Target = TrashAbsorbTarget.Get())
	{
		return Target;
	}
	TInlineComponentArray<UPhysicsHandleComponent*> Handles(GetOwner());
	for (const UPhysicsHandleComponent* Handle : Handles)
	{
		const UPrimitiveComponent* Grabbed = Handle->GetGrabbedComponent();
		AActor* HeldActor = Grabbed ? Grabbed->GetOwner() : nullptr;
		if (!HeldActor)
		{
			continue;
		}
		TInlineComponentArray<USceneComponent*> Components(HeldActor);
		for (USceneComponent* Component : Components)
		{
			if (Component->ComponentHasTag(TrashAbsorbPointTag))
			{
				return Component;
			}
		}
	}
	return nullptr;
}

void UHKCarryComponent::GetIgnoredActors(TArray<AActor*>& IgnoredActors) const
{
	if (const USceneComponent* Target = GetTrashAbsorbTarget())
	{
		IgnoredActors.Add(Target->GetOwner());
	}
}

AActor* UHKCarryComponent::FindInteractionTarget(const FVector& Start, const FVector& End) const
{
	for (TActorIterator<AHKBlanketActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsGrabbed() && It->FindParticleNearRay(Start, End, BlanketGrabRadius) != INDEX_NONE)
		{
			return *It;
		}
	}
	return nullptr;
}

FText UHKCarryComponent::GetBlockingPrompt() const
{
	return NSLOCTEXT("Housekeeping", "CarryReleasePrompt", "[LMB 떼기] 놓기");
}

bool UHKCarryComponent::GetViewTransform(FVector& OutHoldLocation, FRotator& OutViewRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	// GetPawnViewLocation() : 위치 정보 반환 || GetControlRotation() : 회전 정보 반환
	OutViewRotation = Controller->GetControlRotation();
	OutHoldLocation = Pawn->GetPawnViewLocation() + OutViewRotation.Vector() * HoldDistance;
	return true;
}

void UHKCarryComponent::Release(bool bInheritHandVelocity)
{
	UPrimitiveComponent* Released = CarriedComponent;
	CarriedComponent = nullptr;
	
	/*
	blanket
	*/
	if (AHKBlanketActor* Blanket = CarriedBlanket.Get())
	{
		CarriedBlanket.Reset();
		const FVector ThrowVelocity = bInheritHandVelocity ? (HoldVelocity * ThrowVelocityScale).GetClampedToMaxSize(MaxThrowSpeed) : FVector::ZeroVector;
		Blanket->Release(ThrowVelocity);
		
		HoldVelocity = FVector::ZeroVector;
		OnReleased.Broadcast(Released, ThrowVelocity.Size() >= ThrowSpeedThreshold);
		return;
	}
	
	if (PhysicsHandle)
	{
		PhysicsHandle->ReleaseComponent();
	}
	if (!IsValid(Released))
	{
		return;
	}
	Released->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
	bool bThrown = false;
	if (bInheritHandVelocity)
	{
		const FVector ThrowVelocity = (HoldVelocity * ThrowVelocityScale).GetClampedToMaxSize(MaxThrowSpeed);
		Released->SetPhysicsLinearVelocity(ThrowVelocity);
		bThrown = ThrowVelocity.Size() >= ThrowSpeedThreshold;
	}
	HoldVelocity = FVector::ZeroVector;
	OnReleased.Broadcast(Released, bThrown);
	if (AHKPhysicsPickupActor* Pickup = Cast<AHKPhysicsPickupActor>(Released->GetOwner()))
	{
		Pickup->NotifyReleased(bThrown);
	}
}


