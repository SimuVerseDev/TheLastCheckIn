#include "Housekeeping/Objects/Blanket/HKBlanketActor.h"
#include "Housekeeping/Core/Carry/HKCarryComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/PrimitiveComponent.h"

AHKBlanketActor::AHKBlanketActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BlanketMesh"));
	RootComponent = Mesh;
	Mesh->bUseAsyncCooking = true;
	GrabPrompt = NSLOCTEXT("Housekeeping", "BlanketGrapPrompt", "잡기");
}

void AHKBlanketActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitParticles();
	BuildConstraints();
	ComputeParticleNormals();
	BuildMesh(true);
}

void AHKBlanketActor::BeginPlay()
{
	Super::BeginPlay();
	InitParticles();
	BuildConstraints();
}

void AHKBlanketActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DeltaTime <= 0.0f)
	{
		return;
	}
	
	const int32 NumSteps = FMath::Clamp(FMath::CeilToInt(DeltaTime / 0.016f), 1, 4);
	const float SubStep = DeltaTime / NumSteps;
	for (int32 S = 0; S < NumSteps; ++S)
	{
		Step(SubStep);
	}

	ComputeParticleNormals();
	BuildMesh(false);
}

void AHKBlanketActor::InitParticles()
{
	const int32 CountX = SegmentsX + 1;
	const int32 CountY = SegmentsY + 1;
	const int32 Total = CountX * CountY;

	Positions.SetNum(Total);
	PrevPositions.SetNum(Total);
	ParticleNormals.SetNumUninitialized(Total);

	const float DX = Size.X / SegmentsX;
	const float DY = Size.Y / SegmentsY;
	const FVector Origin = FVector(-Size.X * 0.5f, -Size.Y * 0.5f, 0.0f);

	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			const int32 Idx = ToIndex(X, Y);
			const FVector LocalPos = Origin + FVector(X * DX, Y * DY, 0.0f);
			const FVector WorldPos = ActorToWorld().TransformPosition(LocalPos);

			Positions[Idx] = WorldPos;
			PrevPositions[Idx] = WorldPos;
		}
	}
}

void AHKBlanketActor::BuildConstraints()
{
	Constraints.Empty();

	const int32 CountX = SegmentsX + 1;
	const int32 CountY = SegmentsY + 1;

	auto AddConstraint = [this](int32 A, int32 B, float Stiffness)
	{
		FDistanceConstraint C;
		C.A = A;
		C.B = B;
		C.RestLength = FVector::Dist(Positions[A], Positions[B]);
		C.Stiffness = Stiffness;
		Constraints.Add(C);
	};

	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			const int32 Idx = ToIndex(X, Y);

			// Stretch (바로 옆 점)
			if (X + 1 < CountX) AddConstraint(Idx, ToIndex(X + 1, Y), StretchStiffness);
			if (Y + 1 < CountY) AddConstraint(Idx, ToIndex(X, Y + 1), StretchStiffness);

			// Shear (대각선 점)
			if (X + 1 < CountX && Y + 1 < CountY)
			{
				AddConstraint(Idx, ToIndex(X + 1, Y + 1), ShearStiffness);
				AddConstraint(ToIndex(X + 1, Y), ToIndex(X, Y + 1), ShearStiffness);
			}

			// Bend (두 칸 건너 점 - 접힘 방지)
			if (X + 2 < CountX) AddConstraint(Idx, ToIndex(X + 2, Y), BendStiffness);
			if (Y + 2 < CountY) AddConstraint(Idx, ToIndex(X, Y + 2), BendStiffness);
		}
	}
}

void AHKBlanketActor::Step(float Dt)
{
	const int32 Total = Positions.Num();
	const FVector Gravity(0.0f, 0.0f, -980.05f);
	
	for (int32 I = 0; I < Total; ++I)
	{
		if (I == GrabbedIndex)
		{
			continue; // 잡힌 점은 외부에서 제어
		}

		const FVector Temp = Positions[I];
		const FVector Velocity = (Positions[I] - PrevPositions[I]) * (1.0f - Damping);
		Positions[I] += Velocity + Gravity * (Dt * Dt);
		PrevPositions[I] = Temp;
	}
	
	if (GrabbedIndex != INDEX_NONE && Positions.IsValidIndex(GrabbedIndex))
	{
		PrevPositions[GrabbedIndex] = Positions[GrabbedIndex];
		Positions[GrabbedIndex] = GrabTarget;
	}
	
	for (int32 Iter = 0; Iter < SolverIterations; ++Iter)
	{
		SolveConstraints();
		
	}
	SolveCollisions();
}

void AHKBlanketActor::SolveConstraints()
{
	for (const FDistanceConstraint& C : Constraints)
	{
		if (C.A == GrabbedIndex && C.B == GrabbedIndex) continue;

		FVector& PosA = Positions[C.A];
		FVector& PosB = Positions[C.B];

		const FVector Delta = PosB - PosA;
		const float Dist = Delta.Size();
		if (Dist < KINDA_SMALL_NUMBER) continue;

		const float Diff = (Dist - C.RestLength) / Dist;
		const FVector Correction = Delta * (Diff * 0.5f * C.Stiffness);

		if (C.A != GrabbedIndex) PosA += Correction;
		if (C.B != GrabbedIndex) PosB -= Correction;
	}
}

void AHKBlanketActor::SolveCollisions()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	/*
	고정된 물체(ECC_WorldStatic)와 움직이는 물체(ECC_WorldDynamic)를 대상으로 충돌을 감지, 자기자신은 제외
	*/
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HKBlanketCollision), false, this);
	
	const float SurfaceOffset = Thickness * 0.5f + CollisionOffset;

	/*
	각 파티클이 이전 위치에서 현재 위치로 이동한 궤적을 바탕으로 라인 트레이스 발사
	*/
	for (int32 I = 0; I < Positions.Num(); ++I)
	{
		if (I == GrabbedIndex)
		{
			continue;
		}
		FVector& Pos = Positions[I];
		const FVector Start = PrevPositions[I];
		const FVector Move = Pos - Start;
		
		if (Move.IsNearlyZero())
		{
			continue;
		}
		const FVector End = Pos + Move.GetSafeNormal() * SurfaceOffset;

		FHitResult Hit;
		
		bool bContact = World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params);
		if (!bContact)
		{
			const FVector ProbeEnd = Pos - FVector::UpVector * (SurfaceOffset + ContactTolerance);
			bContact = World->LineTraceSingleByObjectType(Hit, Pos, ProbeEnd, ObjectParams, Params);
		}
		if (!bContact)
		{
			continue;
		}
		const FVector Normal = Hit.ImpactNormal;
		FVector NewPos = Pos;
		const float Height = FVector::DotProduct(Pos - Hit.ImpactPoint, Normal);
		if (Height < SurfaceOffset)
		{
			NewPos += Normal * (SurfaceOffset - Height);
		}
		const FVector Slide = FVector::VectorPlaneProject(NewPos - Start, Normal);
		if (Normal.Z >= FloorNormalZ)
		{
			if (Slide.Size() < StaticFrictionDistance)
			{
				NewPos -= Slide;
			}
			else
			{
				NewPos -= Slide * Friction;
			}
		}
		Pos = NewPos;
		PrevPositions[I] = NewPos - FVector::VectorPlaneProject(NewPos - Start, Normal);
	}
}

int32 AHKBlanketActor::FindParticleNearRay(const FVector& RayStart, const FVector& RayEnd, float Radius) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = Radius * Radius;

	for (int32 I = 0; I < Positions.Num(); ++I)
	{
		const FVector& P = Positions[I];
		const float DistSq = FMath::PointDistToSegmentSquared(P, RayStart, RayEnd);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = I;
		}
	}
	return BestIndex;
}

void AHKBlanketActor::Grab(int32 ParticleIndex)
{
	if (Positions.IsValidIndex(ParticleIndex))
	{
		GrabbedIndex = ParticleIndex;
		GrabTarget = Positions[ParticleIndex];
	}
}

void AHKBlanketActor::SetGrabTarget(const FVector& WorldLocation)
{
	GrabTarget = WorldLocation;
}

void AHKBlanketActor::Release(const FVector& Velocity)
{
	if (GrabbedIndex != INDEX_NONE && Positions.IsValidIndex(GrabbedIndex))
	{
		// 놓을 때 속도 반영 (최대 속도 제한)
		const FVector ClampedVel = Velocity.GetClampedToMaxSize(MaxReleaseSpeed);
		PrevPositions[GrabbedIndex] = Positions[GrabbedIndex] - (ClampedVel * 0.016f);
	}
	GrabbedIndex = INDEX_NONE;
}

/*
IHKInteractable
*/
bool AHKBlanketActor::CanInteract_Implementation(AActor* Interactor) const
{
	return false;
}

bool AHKBlanketActor::CanUse_Implementation(AActor* Interactor) const
{
	const UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	return Carry && !Carry->IsCarrying() && !IsGrabbed();
}

FText AHKBlanketActor::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return GrabPrompt;
}

bool AHKBlanketActor::Use_Implementation(AActor* Interactor)
{
	UHKCarryComponent* Carry = Interactor ? Interactor->FindComponentByClass<UHKCarryComponent>() : nullptr;
	return Carry && !IsGrabbed() && Carry->TryPickup(Mesh);
}

void AHKBlanketActor::ComputeParticleNormals()
{
	const int32 CountX = SegmentsX + 1;
	const int32 CountY = SegmentsY + 1;
	ParticleNormals.Init(FVector::UpVector, Positions.Num());

	for (int32 Y = 0; Y < SegmentsY; ++Y)
	{
		for (int32 X = 0; X < SegmentsX; ++X)
		{
			const int32 I00 = ToIndex(X, Y);
			const int32 I10 = ToIndex(X + 1, Y);
			const int32 I01 = ToIndex(X, Y + 1);

			const FVector& P00 = Positions[I00];
			const FVector& P10 = Positions[I10];
			const FVector& P01 = Positions[I01];

			const FVector FaceNormal = (P10 - P00).Cross(P01 - P00).GetSafeNormal();
			ParticleNormals[I00] += FaceNormal;
			ParticleNormals[I10] += FaceNormal;
			ParticleNormals[I01] += FaceNormal;
		}
	}

	const bool bFirst = SmoothedNormals.Num() != ParticleNormals.Num();
	if (bFirst)
	{
		SmoothedNormals.Init(FVector::UpVector, ParticleNormals.Num());
	}

	for (int32 I = 0; I < ParticleNormals.Num(); ++I)
	{
		const FVector Prev = SmoothedNormals[I];
		FVector N = ParticleNormals[I].GetSafeNormal();

		if (N.IsNearlyZero())
		{
			N = Prev;
		}
		else if (FVector::DotProduct(N, Prev) < 0.0f)
		{
			N = -N;
		}

		SmoothedNormals[I] = bFirst ? N : FMath::Lerp(Prev, N, NormalSmoothing).GetSafeNormal(SMALL_NUMBER, N);
		ParticleNormals[I] = SmoothedNormals[I];
	}
}

void AHKBlanketActor::BuildTriangles()
{
	const int32 NumGrid = (SegmentsX + 1) * (SegmentsY + 1);
	MeshTriangles.Reset();

	for (int32 Y = 0; Y < SegmentsY; ++Y)
	{
		for (int32 X = 0; X < SegmentsX; ++X)
		{
			const int32 I00 = ToIndex(X, Y);
			const int32 I10 = ToIndex(X + 1, Y);
			const int32 I01 = ToIndex(X, Y + 1);
			const int32 I11 = ToIndex(X + 1, Y + 1);

			MeshTriangles.Append({ I00, I11, I10, I00, I01, I11 });
			MeshTriangles.Append({ NumGrid + I00, NumGrid + I10, NumGrid + I11, NumGrid + I00, NumGrid + I11, NumGrid + I01 });
		}
	}

	TArray<int32> Border;
	for (int32 X = 0; X < SegmentsX; ++X) Border.Add(ToIndex(X, 0));
	for (int32 Y = 0; Y < SegmentsY; ++Y) Border.Add(ToIndex(SegmentsX, Y));
	for (int32 X = SegmentsX; X > 0; --X) Border.Add(ToIndex(X, SegmentsY));
	for (int32 Y = SegmentsY; Y > 0; --Y) Border.Add(ToIndex(0, Y));

	for (int32 I = 0; I < Border.Num(); ++I)
	{
		const int32 A = Border[I];
		const int32 B = Border[(I + 1) % Border.Num()];
		MeshTriangles.Append({ A, B, NumGrid + A, B, NumGrid + B, NumGrid + A });
	}
}

void AHKBlanketActor::BuildMesh(bool bCreate)
{
	const int32 CountX = SegmentsX + 1;
	const int32 CountY = SegmentsY + 1;
	const int32 NumGrid = CountX * CountY;
	const float HalfThickness = Thickness * 0.5f;
	const FTransform WorldToLocal = ActorToWorld().Inverse();

	MeshVertices.SetNum(NumGrid * 2);
	MeshNormals.SetNum(NumGrid * 2);
	MeshUVs.SetNum(NumGrid * 2);

	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			const int32 Idx = ToIndex(X, Y);
			const FVector Normal = ParticleNormals[Idx];
			const FVector2D UV(static_cast<float>(X) / SegmentsX, static_cast<float>(Y) / SegmentsY);

			MeshVertices[Idx] = WorldToLocal.TransformPosition(Positions[Idx] + Normal * HalfThickness);
			MeshNormals[Idx] = WorldToLocal.TransformVectorNoScale(Normal);
			MeshUVs[Idx] = UV;

			MeshVertices[NumGrid + Idx] = WorldToLocal.TransformPosition(Positions[Idx] - Normal * HalfThickness);
			MeshNormals[NumGrid + Idx] = -MeshNormals[Idx];
			MeshUVs[NumGrid + Idx] = UV;
		}
	}

	if (bCreate || MeshTriangles.Num() == 0)
	{
		BuildTriangles();
	}

	if (bCreate || Mesh->GetNumSections() == 0)
	{
		Mesh->CreateMeshSection(0, MeshVertices, MeshTriangles, MeshNormals, MeshUVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (Material)
		{
			Mesh->SetMaterial(0, Material);
		}
	}
	else
	{
		Mesh->UpdateMeshSection(0, MeshVertices, MeshNormals, MeshUVs, TArray<FColor>(), TArray<FProcMeshTangent>());
	}
}

void AHKBlanketActor::Scramble(FRandomStream& Random, const FVector& Center, float MaxOffset)
{
	GrabbedIndex = INDEX_NONE;

	const int32 LayersX = Random.RandRange(1, 3);
	const int32 LayersY = Random.RandRange(1, 2);
	const float FoldX = Size.X / LayersX;
	const float FoldY = Size.Y / LayersY;
	const float DX = Size.X / SegmentsX;
	const float DY = Size.Y / SegmentsY;
	const float LayerGap = Thickness * 0.5f;

	const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(Random.FRandRange(0.0f, 360.0f)));
	const FVector2D Offset2D = FVector2D(Random.FRandRange(-1.0f, 1.0f), Random.FRandRange(-1.0f, 1.0f)) * MaxOffset;
	const FVector Base = Center + FVector(Offset2D, 0.0f);

	/*
	아코디언 접기 : 홀수 겹은 좌표를 뒤집어서 접힌 면이 이어지게 함 (점 사이 거리 유지)
	*/
	auto Fold = [](float Coord, float FoldSize, int32 NumLayers, int32& OutLayer)
	{
		OutLayer = FMath::Min(FMath::FloorToInt(Coord / FoldSize), NumLayers - 1);
		const float Local = Coord - OutLayer * FoldSize;
		return (OutLayer % 2 == 1) ? FoldSize - Local : Local;
	};

	for (int32 Y = 0; Y <= SegmentsY; ++Y)
	{
		for (int32 X = 0; X <= SegmentsX; ++X)
		{
			int32 LayerX = 0;
			int32 LayerY = 0;
			const float U = Fold(X * DX, FoldX, LayersX, LayerX);
			const float V = Fold(Y * DY, FoldY, LayersY, LayerY);
			const int32 Layer = LayerY * LayersX + LayerX;

			const FVector Local(U - FoldX * 0.5f, V - FoldY * 0.5f, Layer * LayerGap);
			const int32 Idx = ToIndex(X, Y);
			Positions[Idx] = Base + Yaw.RotateVector(Local);
			PrevPositions[Idx] = Positions[Idx];
		}
	}
}