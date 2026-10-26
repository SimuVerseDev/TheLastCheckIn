#include "Housekeeping/Core/Room/HKRoomVolume.h"
#include "Housekeeping/Core/Dirt/HKDirtComponent.h"
#include "Housekeeping/Objects/Trash/HKTrashActor.h"
#include "Housekeeping/Objects/Trash/HKTrashDirtComponent.h"
#include "Housekeeping/Objects/Trash/HKTrashPoolSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

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
}

void AHKRoomVolume::BeginPlay()
{
	Super::BeginPlay();
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
	SpawnTrash(Random);
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