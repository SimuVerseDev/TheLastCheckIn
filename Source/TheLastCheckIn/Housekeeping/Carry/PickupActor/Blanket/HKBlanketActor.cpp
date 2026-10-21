#include "HKBlanketActor.h"
#include "ProceduralMeshComponent.h"
#include "Components/PrimitiveComponent.h"

AHKBlanketActor::AHKBlanketActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BlanketMesh"));
	RootComponent = Mesh;
	Mesh->bUseAsyncCooking = true;
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

	// 고정된 서브스텝 (안정적인 물리 시뮬레이션을 위해 분할)
	TimeAccumulator += DeltaTime;
	const float SubStep = 0.016f; // 약 60fps 기준

	while (TimeAccumulator >= SubStep)
	{
		Step(SubStep);
		TimeAccumulator -= SubStep;
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

			// 1. Stretch (바로 옆 점)
			if (X + 1 < CountX) AddConstraint(Idx, ToIndex(X + 1, Y), StretchStiffness);
			if (Y + 1 < CountY) AddConstraint(Idx, ToIndex(X, Y + 1), StretchStiffness);

			// 2. Shear (대각선 점)
			if (X + 1 < CountX && Y + 1 < CountY)
			{
				AddConstraint(Idx, ToIndex(X + 1, Y + 1), ShearStiffness);
				AddConstraint(ToIndex(X + 1, Y), ToIndex(X, Y + 1), ShearStiffness);
			}

			// 3. Bend (두 칸 건너 점 - 접힘 방지)
			if (X + 2 < CountX) AddConstraint(Idx, ToIndex(X + 2, Y), BendStiffness);
			if (Y + 2 < CountY) AddConstraint(Idx, ToIndex(X, Y + 2), BendStiffness);
		}
	}
}

void AHKBlanketActor::Step(float Dt)
{
	const int32 Total = Positions.Num();
	const FVector Gravity(0.0f, 0.0f, -980.05f); // cm 단위 중력

	// 1. Verlet Integration (위치 업데이트 및 중력 적용)
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

	// 잡힌 점 위치 강제 고정
	if (GrabbedIndex != INDEX_NONE && Positions.IsValidIndex(GrabbedIndex))
	{
		PrevPositions[GrabbedIndex] = Positions[GrabbedIndex];
		Positions[GrabbedIndex] = GrabTarget;
	}

	// 2. Constraints & Collisions 반복 해결
	for (int32 Iter = 0; Iter < SolverIterations; ++Iter)
	{
		SolveConstraints();
		SolveCollisions();
	}
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
	// 월드 레이캐스트 등을 통한 간단한 바닥/침대 충돌 처리 자리 (기본 높이 Z=0 평면 예시 혹은 콜리전 탐색)
	const int32 Total = Positions.Num();
	for (int32 I = 0; I < Total; ++I)
	{
		if (I == GrabbedIndex) continue;

		FVector& Pos = Positions[I];
		// 단순 바닥 예시 (추후 침대 높이나 바닥 높이에 맞게 조정 가능)
		if (Pos.Z < 0.0f)
		{
			const FVector Prev = PrevPositions[I];
			Pos.Z = 0.0f;
			// 마찰력 적용
			FVector TangentVel = (Pos - Prev);
			TangentVel.Z = 0.0f;
			Pos -= TangentVel * Friction;
		}
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

	for (int32 I = 0; I < ParticleNormals.Num(); ++I)
	{
		ParticleNormals[I] = ParticleNormals[I].GetSafeNormal();
	}
}

void AHKBlanketActor::BuildMesh(bool bCreate)
{
	const int32 CountX = SegmentsX + 1;
	const int32 CountY = SegmentsY + 1;
	const int32 NumVerts = CountX * CountY;

	MeshVertices.SetNum(NumVerts);
	MeshNormals.SetNum(NumVerts);
	MeshUVs.SetNum(NumVerts);

	const FTransform LocalTransform = ActorToWorld().Inverse();

	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			const int32 Idx = ToIndex(X, Y);
			const FVector WorldPos = Positions[Idx];
			
			MeshVertices[Idx] = LocalTransform.TransformPosition(WorldPos);
			MeshNormals[Idx] = LocalTransform.TransformVector(ParticleNormals[Idx]);
			MeshUVs[Idx] = FVector2D(static_cast<float>(X) / SegmentsX, static_cast<float>(Y) / SegmentsY);
		}
	}

	if (bCreate || MeshTriangles.Num() == 0)
	{
		MeshTriangles.Empty();
		for (int32 Y = 0; Y < SegmentsY; ++Y)
		{
			for (int32 X = 0; X < SegmentsX; ++X)
			{
				const int32 I00 = ToIndex(X, Y);
				const int32 I10 = ToIndex(X + 1, Y);
				const int32 I01 = ToIndex(X, Y + 1);
				const int32 I11 = ToIndex(X + 1, Y + 1);

				// Triangle 1
				MeshTriangles.Add(I00);
				MeshTriangles.Add(I10);
				MeshTriangles.Add(I11);

				// Triangle 2
				MeshTriangles.Add(I00);
				MeshTriangles.Add(I11);
				MeshTriangles.Add(I01);
			}
		}
	}

	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	Mesh->CreateMeshSection(0, MeshVertices, MeshTriangles, MeshNormals, MeshUVs, VertexColors, Tangents, false);

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
}