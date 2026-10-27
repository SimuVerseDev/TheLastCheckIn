#include "Housekeeping/Objects/Blanket/HKBedMakeEvaluator.h"
#include "Housekeeping/Objects/Blanket/HKBlanketActor.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogHKBedMake, Log, All);

UHKBedMakeEvaluator::UHKBedMakeEvaluator()
{
	Tolerance = 0.08f;
}

void UHKBedMakeEvaluator::BeginPlay()
{
	Super::BeginPlay();

	TArray<UBoxComponent*> Boxes;
	GetOwner()->GetComponents<UBoxComponent>(Boxes);
	for (UBoxComponent* Box : Boxes)
	{
		if (Box->ComponentHasTag(BedTopTag))
		{
			BedArea = Box;
			break;
		}
	}
	if (!BedArea)
	{
		UE_LOG(LogHKBedMake, Warning, TEXT("%s : '%s' 태그 Box 컴포넌트 없음"), *GetOwner()->GetName(), *BedTopTag.ToString());
	}

	if (!Blanket)
	{
		float BestDistSq = AutoFindRadius * AutoFindRadius;
		for (TActorIterator<AHKBlanketActor> It(GetWorld()); It; ++It)
		{
			const float DistSq = FVector::DistSquared(It->GetActorLocation(), GetOwner()->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Blanket = *It;
			}
		}
	}
}

float UHKBedMakeEvaluator::EvaluateDirt() const
{
	if (!Blanket || !BedArea)
	{
		return 0.0f;
	}

	const FTransform BoxTransform = BedArea->GetComponentTransform();
	const FVector Extent = BedArea->GetUnscaledBoxExtent();
	const int32 NumCells = CoverGridX * CoverGridY;

	TArray<int32> CellCounts;
	CellCounts.Init(0, NumCells);
	int32 NumInside = 0;
	float SumZ = 0.0f;
	float SumZSq = 0.0f;

	for (const FVector& P : Blanket->GetParticlePositions())
	{
		const FVector L = BoxTransform.InverseTransformPosition(P);
		if (FMath::Abs(L.X) > Extent.X || FMath::Abs(L.Y) > Extent.Y)
		{
			continue;
		}

		++NumInside;
		SumZ += L.Z;
		SumZSq += L.Z * L.Z;

		if (FMath::Abs(L.Z) <= Extent.Z)
		{
			const int32 CX = FMath::Clamp(FMath::FloorToInt((L.X + Extent.X) / (2.0f * Extent.X) * CoverGridX), 0, CoverGridX - 1);
			const int32 CY = FMath::Clamp(FMath::FloorToInt((L.Y + Extent.Y) / (2.0f * Extent.Y) * CoverGridY), 0, CoverGridY - 1);
			++CellCounts[CY * CoverGridX + CX];
		}
	}

	int32 NumCovered = 0;
	for (const int32 Count : CellCounts)
	{
		NumCovered += Count >= MinParticlesPerCell ? 1 : 0;
	}
	LastCoverRatio = static_cast<float>(NumCovered) / NumCells;

	LastFlatness = 0.0f;
	if (NumInside > 1)
	{
		const float Mean = SumZ / NumInside;
		const float StdDev = FMath::Sqrt(FMath::Max(SumZSq / NumInside - Mean * Mean, 0.0f));
		LastFlatness = 1.0f - FMath::Clamp(StdDev / MaxHeightDeviation, 0.0f, 1.0f);
	}

	return 1.0f - (LastCoverRatio * CoverWeight + LastFlatness * (1.0f - CoverWeight));
}

void UHKBedMakeEvaluator::ApplyDirt(FRandomStream& Random)
{
	Super::ApplyDirt(Random);

	if (!Blanket || !BedArea)
	{
		return;
	}
	
	const FVector Top = BedArea->GetComponentLocation() + BedArea->GetUpVector() * (BedArea->GetScaledBoxExtent().Z + 20.0f);
	Blanket->Scramble(Random, Top, ScatterOffset);
	GetWorld()->GetTimerManager().SetTimer(MadeCheckTimer, this, &UHKBedMakeEvaluator::CheckBedMade, MadeCheckInterval, true);
}

void UHKBedMakeEvaluator::ClearDirt()
{
	Super::ClearDirt();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MadeCheckTimer);
	}
}

void UHKBedMakeEvaluator::CheckBedMade()
{
	if (!IsDirtActive() || !Blanket || Blanket->IsGrabbed())
	{
		return;
	}
	if (GetRawDirt() > Tolerance)
	{
		return;
	}
	ClearDirt();
	if (!bAutoMakeBed || !BedArea)
	{
		return;
	}
	const FVector Location = BedArea->GetComponentLocation() + BedArea->GetUpVector() * BedArea->GetScaledBoxExtent().Z;
	const FQuat Rotation = BedArea->GetComponentQuat() * FQuat(FVector::UpVector, FMath::DegreesToRadians(MadeBlanketYaw));
	Blanket->MakeBed(FTransform(Rotation, Location), MakeBedDuration);
}
