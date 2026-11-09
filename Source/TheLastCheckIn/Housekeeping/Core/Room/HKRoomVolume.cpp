#include "Housekeeping/Core/Room/HKRoomVolume.h"
#include "Housekeeping/Core/Dirt/HKDirtComponent.h"
#include "Housekeeping/Objects/Trash/HKTrashActor.h"
#include "Housekeeping/Objects/Trash/HKTrashDirtComponent.h"
#include "Housekeeping/Objects/Trash/HKTrashPoolSubsystem.h"
#include "Housekeeping/Objects/Stain/HKStainActor.h"
#include "Housekeeping/Objects/Stain/HKStainDirtComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Rooms/RoomActor.h"

AHKRoomVolume::AHKRoomVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	/*
	박스 크기, 충돌 설정, 시각화 색상의 기본값은 생성자에서 세팅한다.
	*/
	Bounds->SetBoxExtent(FVector(300.0f, 300.0f, 150.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->ShapeColor = FColor(80, 200, 255);

	static ConstructorHelpers::FClassFinder<AHKStainActor> StainFinder(TEXT("/Game/HouseKeeping/Blueprints/BP_HK_Stain"));
	if (StainFinder.Succeeded())
	{
		StainClass = StainFinder.Class;
	}
}

void AHKRoomVolume::BeginPlay()
{
	Super::BeginPlay();
	if (LinkedRoom)
	{
		LinkedRoom->OnAssigned.AddDynamic(this, &AHKRoomVolume::HandleRoomAssigned);
		LinkedRoom->OnCheckedOut.AddDynamic(this, &AHKRoomVolume::HandleRoomCheckedOut);
	}
	if (bDirtyOnBeginPlay)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AHKRoomVolume::ApplyRandomDirt);
	}
}

void AHKRoomVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EvaluateTimer);
	Super::EndPlay(EndPlayReason);
}

bool AHKRoomVolume::IsPointInside(const FVector& WorldLocation) const
{
	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AHKRoomVolume::ApplyRandomDirt()
{
	ClearDirt();
	FRandomStream Random;
	Random.GenerateNewSeed();
	
	TArray<UHKDirtComponent*> Candidates;
	CollectCandidates(Candidates);
	
	/* 앞에서부터 N개 선택 */
	for (int32 I = Candidates.Num() - 1; I > 0; --I)
	{
		Candidates.Swap(I, Random.RandRange(0, I));
	}
	const int32 NumSources = FMath::Min(Random.RandRange(MinDirtSources, FMath::Max(MinDirtSources, MaxDirtSources)), Candidates.Num());
	for (int32 I = 0; I < NumSources; ++I)
	{
		Candidates[I]->ApplyDirt(Random);
		ActiveDirt.Add(Candidates[I]);
		TotalWeight += Candidates[I]->GetWeight();
	}
	//
	SpawnTrash(Random);
	SpawnStains(Random);
	//
	bDirty = TotalWeight > 0.0f;
	if (bDirty)
	{
		GetWorldTimerManager().SetTimer(EvaluateTimer, this, &AHKRoomVolume::Evaluate, EvaluateInterval, true);
		Evaluate();
	}
}

void AHKRoomVolume::ClearDirt()
{
	UHKTrashPoolSubsystem* Pool = GetWorld()->GetSubsystem<UHKTrashPoolSubsystem>();
	for (const TWeakObjectPtr<UHKDirtComponent>& Weak : ActiveDirt)
	{
		UHKDirtComponent* Dirt = Weak.Get();
		if (!Dirt || !IsCountedHere(Dirt))
		{
			continue;
		}
		if (AHKTrashActor* Trash = Cast<AHKTrashActor>(Dirt->GetOwner()); Trash && Pool)
		{
			Pool->ReturnToPool(Trash);
		}
		Dirt->ClearDirt();
	}
	for (AHKStainActor* SpawnedStain : SpawnedStains)
	{
		if (IsValid(SpawnedStain))
		{
			SpawnedStain->Destroy();
		}
	}
	SpawnedStains.Reset();
	ActiveDirt.Reset();
	TotalWeight = 0.0f;
	bDirty = false;
	GetWorldTimerManager().ClearTimer(EvaluateTimer);
	SetCleanliness(100.0f);
}

void AHKRoomVolume::CollectCandidates(TArray<UHKDirtComponent*>& OutCandidates) const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UHKDirtComponent*> Components;
		It->GetComponents(Components);
		for (UHKDirtComponent* Dirt : Components)
		{
			if (Dirt->IsRandomCandidate() && IsPointInside(Dirt->GetHomeTransform().GetLocation()))
			{
				OutCandidates.Add(Dirt);
			}
		}
	}
}

void AHKRoomVolume::SpawnTrash(FRandomStream& Random)
{
	UHKTrashPoolSubsystem* Pool = GetWorld()->GetSubsystem<UHKTrashPoolSubsystem>();
	if (!Pool || TrashClasses.Num() == 0)
	{
		return;
	}
	const int32 NumTrash = Random.RandRange(MinTrash, FMath::Max(MinTrash, MaxTrash));
	for (int32 I = 0; I < NumTrash; ++I)
	{
		const TSubclassOf<AHKTrashActor> TrashClass = TrashClasses[Random.RandRange(0, TrashClasses.Num() - 1)];
		FTransform SpawnTransform;
		if (!TrashClass || !FindTrashSpawnTransform(Random, SpawnTransform))
		{
			continue;
		}
		AHKTrashActor* Trash = Pool->GetTrash(TrashClass, SpawnTransform);
		if (!Trash)
		{
			continue;
		}
		UHKTrashDirtComponent* TrashDirt = Trash->GetDirtComponent();
		TrashDirt->SetRoomBounds(Bounds);
		TrashDirt->ApplyDirt(Random);
		ActiveDirt.Add(TrashDirt);
		TotalWeight += TrashDirt->GetWeight();
	}
}

bool AHKRoomVolume::FindTrashSpawnTransform(FRandomStream& Random, FTransform& OutTransform) const
{
	FVector Location;
	if (TrashSpawnPoints.Num() > 0)
	{
		const AActor* Point = TrashSpawnPoints[Random.RandRange(0, TrashSpawnPoints.Num() - 1)];
		if (!Point)
		{
			return false;
		}
		Location = Point->GetActorLocation();
	}
	else
	{
		const FVector Extent = Bounds->GetScaledBoxExtent();
		const FVector LocalXY(Random.FRandRange(-Extent.X, Extent.X) * 0.8f, Random.FRandRange(-Extent.Y, Extent.Y) * 0.8f, 0.0f);
		const FVector Center = Bounds->GetComponentLocation() + Bounds->GetComponentQuat().RotateVector(LocalXY);
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(HKTrashSpawn), false, this);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Center + FVector(0.0f, 0.0f, Extent.Z), Center - FVector(0.0f, 0.0f, Extent.Z), ECC_Visibility, Params))
		{
			return false;
		}
		Location = Hit.ImpactPoint;
	}
	OutTransform = FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Location + FVector(0.0f, 0.0f, 15.0f));
	return true;
}

void AHKRoomVolume::SpawnStains(FRandomStream& Random)
{
	if (!StainClass)
	{
		return;
	}
	float FloorZ = 0.0f;
	if (!FindFloorZ(FloorZ))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const int32 NumStains = Random.RandRange(MinStains, FMath::Max(MinStains, MaxStains));
	for (int32 I = 0; I < NumStains; ++I)
	{
		FHitResult Hit;
		if (!FindFloorPoint(Random, FloorZ, Hit))
		{
			continue;
		}
		const FRotator Rotation(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f);
		AHKStainActor* NewStain = GetWorld()->SpawnActor<AHKStainActor>(StainClass, Hit.ImpactPoint, Rotation, Params);
		if (!NewStain)
		{
			continue;
		}
		UHKStainDirtComponent* StainDirt = NewStain->GetStain();
		StainDirt->ApplyDirt(Random);
		SpawnedStains.Add(NewStain);
		ActiveDirt.Add(StainDirt);
		TotalWeight += StainDirt->GetWeight();
	}
}

bool AHKRoomVolume::FindFloorPoint(FRandomStream& Random, float FloorZ, FHitResult& OutHit) const
{
	const FVector Extent = Bounds->GetScaledBoxExtent();
	for (int32 Attempt = 0; Attempt < StainSpawnAttempts; ++Attempt)
	{
		const FVector LocalXY(Random.FRandRange(-Extent.X, Extent.X) * 0.8f, Random.FRandRange(-Extent.Y, Extent.Y) * 0.8f, 0.0f);
		if (!TraceStainSurface(LocalXY, OutHit))
		{
			continue;
		}
		const AActor* HitActor = OutHit.GetActor();
		if (OutHit.ImpactPoint.Z > FloorZ + FloorHeightTolerance
			|| (HitActor && HitActor->FindComponentByClass<UHKDirtComponent>()))
		{
			continue;
		}
		return true;
	}
	return false;
}

bool AHKRoomVolume::FindFloorZ(float& OutFloorZ) const
{
	/*
	가구는 항상 바닥 위에 있으므로, 격자로 쏜 트레이스 중 가장 낮은 평평한 면을 바닥 높이로 사용
	*/
	constexpr int32 GridSize = 6;
	const FVector Extent = Bounds->GetScaledBoxExtent();
	bool bFound = false;
	for (int32 X = 0; X < GridSize; ++X)
	{
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			const float U = FMath::Lerp(-0.8f, 0.8f, static_cast<float>(X) / (GridSize - 1));
			const float V = FMath::Lerp(-0.8f, 0.8f, static_cast<float>(Y) / (GridSize - 1));
			FHitResult Hit;
			if (TraceStainSurface(FVector(Extent.X * U, Extent.Y * V, 0.0f), Hit))
			{
				OutFloorZ = bFound ? FMath::Min(OutFloorZ, Hit.ImpactPoint.Z) : Hit.ImpactPoint.Z;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool AHKRoomVolume::TraceStainSurface(const FVector& LocalXY, FHitResult& OutHit) const
{
	const FVector Extent = Bounds->GetScaledBoxExtent();
	const FVector Center = Bounds->GetComponentLocation() + Bounds->GetComponentQuat().RotateVector(LocalXY);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HKStainSpawn), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(OutHit, Center + FVector(0.0f, 0.0f, Extent.Z), Center - FVector(0.0f, 0.0f, Extent.Z), ECC_Visibility, Params))
	{
		return false;
	}
	const UPrimitiveComponent* HitComponent = OutHit.GetComponent();
	return OutHit.ImpactNormal.Z >= 0.9f && !(HitComponent && HitComponent->IsSimulatingPhysics());
}

bool AHKRoomVolume::IsCountedHere(const UHKDirtComponent* Dirt) const
{
	if (const UHKTrashDirtComponent* TrashDirt = Cast<UHKTrashDirtComponent>(Dirt))
	{
		return TrashDirt->GetRoomBounds() == Bounds;
	}
	return true;
}

void AHKRoomVolume::Evaluate()
{
	if (!bDirty || TotalWeight <= 0.0f)
	{
		return;
	}
	float WeightedDirt = 0.0f;
	for (const TWeakObjectPtr<UHKDirtComponent>& Weak : ActiveDirt)
	{
		const UHKDirtComponent* Dirt = Weak.Get();
		if (Dirt && IsCountedHere(Dirt))
		{
			WeightedDirt += Dirt->GetWeight() * Dirt->GetDirt();
		}
	}
	SetCleanliness(100.0f * (1.0f - FMath::Clamp(WeightedDirt / TotalWeight, 0.0f, 1.0f)));
	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), EvaluateInterval + 0.1f, FColor::Cyan,
			FString::Printf(TEXT("[%s] 청결도 %.0f"), *GetActorNameOrLabel(), Cleanliness));
	}
	if (WeightedDirt <= 0.0f)
	{
		FinishCleaning();
	}
}

void AHKRoomVolume::FinishCleaning()
{
	for (const TWeakObjectPtr<UHKDirtComponent>& Weak : ActiveDirt)
	{
		if (UHKDirtComponent* Dirt = Weak.Get(); Dirt && IsCountedHere(Dirt))
		{
			Dirt->ClearDirt();
		}
	}
	ActiveDirt.Reset();
	TotalWeight = 0.0f;
	bDirty = false;
	GetWorldTimerManager().ClearTimer(EvaluateTimer);
	SetCleanliness(100.0f);
	OnRoomCleaned.Broadcast(this);
	
	/*
	DEBUG
	*/
	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 3.0f, FColor::Green,
			FString::Printf(TEXT("[%s] 청결도 100 - 정리 완료"), *GetActorNameOrLabel()));
	}
}

void AHKRoomVolume::SetCleanliness(float NewCleanliness)
{
	if (!FMath::IsNearlyEqual(Cleanliness, NewCleanliness, 0.1f))
	{
		Cleanliness = NewCleanliness;
		OnCleanlinessChanged.Broadcast(this, Cleanliness);
	}
}

void AHKRoomVolume::HandleRoomAssigned(ARoomActor* Room)
{
	if (bDirty)
	{
		// TODO: 평판 시스템 연결 - 청결도에 따라 평판 하락
		UE_LOG(LogTemp, Warning, TEXT("[%s] 미청소 객실 %d 배정 (청결도 %.0f)"),
			*GetActorNameOrLabel(), Room->GetRoomNumber(), Cleanliness);
	}
}

void AHKRoomVolume::HandleRoomCheckedOut(ARoomActor* Room)
{
	ApplyRandomDirt();
}