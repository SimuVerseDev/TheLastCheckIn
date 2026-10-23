#include "Housekeeping/Cleaning/HKPlacementDirtComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogHKPlacementDirt, Log, All);

void UHKPlacementDirtComponent::BeginPlay()
{
	Super::BeginPlay();
	
	const USceneComponent* Root = GetOwner()->GetRootComponent();
	if (Root && Root -> Mobility != EComponentMobility::Movable)
	{
		UE_LOG(LogHKPlacementDirt, Warning, TEXT("%s : 루트가 Movable이 아니라 흐트러뜨릴 수 없음"), *GetOwner()->GetName());
	}
}

float UHKPlacementDirtComponent::EvaluateDirt() const
{
	const FTransform& Home = GetHomeTransform();
	const FTransform Current = GetOwner()->GetActorTransform();

	const float DistanceDirt = FVector::Dist(Current.GetLocation(), Home.GetLocation()) / MaxDistance;
	const float AngleDirt = FMath::RadiansToDegrees(Current.GetRotation().AngularDistance(Home.GetRotation())) / MaxAngle;

	return FMath::Max(DistanceDirt, AngleDirt);
}

void UHKPlacementDirtComponent::ApplyDirt(FRandomStream& Random)
{
	Super::ApplyDirt(Random);
	
	AActor* Owner = GetOwner();
	const FTransform& Home = GetHomeTransform();
	
	FRotator NewRotation = Home.Rotator();
	NewRotation.Yaw += Random.FRandRange(-ScatterMaxYaw, ScatterMaxYaw);

	float Lift = 5.0f;
	if (Random.FRand() < TipOverChance)
	{
		NewRotation.Roll += Random.FRand() < 0.5f ? 90.0f : -90.0f;
		Lift += Owner->GetComponentsBoundingBox().GetExtent().GetMax();
	}
	
	const FVector Direction = FVector::ForwardVector.RotateAngleAxis(Random.FRandRange(0.0f, 360.0f), FVector::UpVector);
	const FVector Offset = Direction * Random.FRandRange(ScatterMinOffset, ScatterMaxOffset);
	const FVector Start = Home.GetLocation() + FVector(0.0f, 0.0f, Lift);
	
	Owner->SetActorLocationAndRotation(Start, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Owner->SetActorLocation(Start + Offset, true, nullptr, ETeleportType::TeleportPhysics);
	
	/*
	물리속도 초기화
	*/
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		if (Prim->IsSimulatingPhysics())
		{
			Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Prim->WakeAllRigidBodies();
		}
	}
}