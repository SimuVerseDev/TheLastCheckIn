#include "Housekeeping/Core/Dirt/HKPlacementDirtComponent.h"
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
	/*
	스냅 중이면 취소하고 물리 복구
	*/
	if (SnapTimer.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(SnapTimer);
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
		{
			Prim->SetSimulatePhysics(true);
		}
	}
	
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

bool UHKPlacementDirtComponent::TrySnapHome()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!Prim || SnapTimer.IsValid())
	{
		return false;
	}

	const FTransform& Home = GetHomeTransform();
	const FTransform Current = Owner->GetActorTransform();
	/*
	들고 있는 높이·시선 방향과 무관하게 - 수평 거리와 기울기만 검사
	*/
	const FVector Delta = Current.GetLocation() - Home.GetLocation();
	const float Distance = Delta.Size2D();
	const float Height = FMath::Abs(Delta.Z);
	const float Tilt = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Current.GetRotation().GetUpVector(), Home.GetRotation().GetUpVector()), -1.0f, 1.0f)));
	if (Distance > SnapDistance || Height > SnapHeight || Tilt > SnapAngle)
	{
		return false;
	}

	/*
	이동 중엔 물리 끄기 - 끝나면 다시 켬
	*/
	Prim->SetSimulatePhysics(false);
	SnapStart = Current;
	SnapStartTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(SnapTimer, this, &UHKPlacementDirtComponent::TickSnap, 0.01f, true);
	return true;
}

void UHKPlacementDirtComponent::TickSnap()
{
	AActor* Owner = GetOwner();
	const FTransform& Home = GetHomeTransform();
	const float Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - SnapStartTime) / SnapDuration, 0.0f, 1.0f);
	const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

	const FVector Location = FMath::Lerp(SnapStart.GetLocation(), Home.GetLocation(), Eased);
	const FQuat Rotation = FQuat::Slerp(SnapStart.GetRotation(), Home.GetRotation(), Eased);
	Owner->SetActorLocationAndRotation(Location, Rotation);

	if (Alpha < 1.0f)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(SnapTimer);
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		Prim->SetSimulatePhysics(true);
		Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}
