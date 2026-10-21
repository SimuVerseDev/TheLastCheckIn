#include "Housekeeping/Carry/HKCarryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Housekeeping/Carry/PickupActor/HKPhysicsPickupActor.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Housekeeping/Carry/PickupActor/Blanket/HKBlanketActor.h"

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
	if (!IsValid(CarriedComponent) || !CarriedComponent->IsSimulatingPhysics() || !GetViewTransform(HoldLocation, ViewRotation))
	{
		Release(false);
		return;
	}
	
	/*
	if picking blanket
	*/
	if (CarriedBlanket.IsValid())
	{
		if (FVector::Dist(CarriedComponent->GetComponentLocation(), HoldLocation) > BreakDistance * 1.5f)
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
		CarriedBlanket->SetGrabTarget(HoldLocation);
		return;
	}
	// else
	if (!IsValid(CarriedComponent) || !CarriedComponent->IsSimulatingPhysics())
	{
		Release(false);
		return;
	}
	
	if (FVector::Dist(CarriedComponent->GetComponentLocation(), HoldLocation) > BreakDistance)
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
		const FVector RayStart = HoldLocation;
		const FVector RayEnd = RayStart + ViewRotation.Vector() * 300.0f; // * 기호 확인
		
		int32 ParticleIndex = Blanket->FindParticleNearRay(RayStart, RayEnd, 50.0f);
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


