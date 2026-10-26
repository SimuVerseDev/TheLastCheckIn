#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Housekeeping/Core/Interaction/HKInteractable.h"
#include "HKBlanketActor.generated.h"
class UProceduralMeshComponent;
class UMaterialInterface;
struct FCollisionObjectQueryParams;
struct FCollisionQueryParams;
UCLASS()
class AHKBlanketActor : public AActor, public IHKInteractable
{
	GENERATED_BODY()
public:
	AHKBlanketActor();
	virtual void Tick(float DeltaTime) override;
	int32 FindParticleNearRay(const FVector& RayStart, const FVector& RayEnd, float Radius) const;
	void Grab(int32 ParticleIndex);
	void SetGrabTarget(const FVector& WorldLocation);
	void Release(const FVector& Velocity);
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Blanket")
	bool IsGrabbed() const
	{
		return GrabbedIndex != INDEX_NONE;
	}
	FVector GetParticleLocation(int32 ParticleIndex) const
	{
		return Positions.IsValidIndex(ParticleIndex) ? Positions[ParticleIndex] : GetActorLocation();
	}
	FText GetGrabPrompt() const
	{
		return GrabPrompt;
	}
	
	const TArray<FVector>& GetParticlePositions() const
	{
		return Positions;
	}
	void Scramble(FRandomStream& Random, const FVector& Center, float MaxOffset);

	/*
	IHKInteractable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool CanUse_Implementation(AActor* Interactor) const override;
	virtual FText GetUsePrompt_Implementation(AActor* Interactor) const override;
	virtual bool Use_Implementation(AActor* Interactor) override;

protected:
	/*
	유연한 오브젝트를 프로시저럴(절차적)로 생성하고 물리 시뮬레이션(스프링-댐퍼 방식의 천 시뮬레이션)과 상호작용하기 위한 설정 값들을 정의
	*/
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Shape", meta=(Units="cm"))
	FVector2D Size = FVector2D(200.0f, 220.0f);
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Shape", meta=(ClampMin=2, ClampMax=64))
	int32 SegmentsX = 20;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Shape", meta=(ClampMin=2, ClampMax=64))
	int32 SegmentsY = 22;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Shape", meta=(ClampMin=0, Units="cm"))
	float Thickness = 25.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Shape")
	TObjectPtr<UMaterialInterface> Material;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=1, ClampMax=32))
	int32 SolverIterations = 16;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float Damping = 0.08f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float StretchStiffness = 1.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float ShearStiffness = 0.8f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float BendStiffness = 0.5f;
	
	/*
    Friction
    */
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float Friction = 0.6f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float FloorNormalZ = 0.7f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0.01, ClampMax=1))
	float NormalSmoothing = 0.35f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, Units="cm"))
	float StaticFrictionDistance = 0.5f;
    
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, Units="cm"))
	float ContactTolerance = 1.0f;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, Units="cm/s"))
	float MaxReleaseSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=100, Units="cm/s"))
	float MaxParticleSpeed = 2000.0f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float CompressionStiffness = 0.1f;

	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=100, Units="cm/s"))
	float MaxGrabSpeed = 1200.0f;

	/*
	잡은 점 기준 원래 거리의 몇 배까지 멀어질 수 있는지 (테더)
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=1))
	float TetherSlack = 1.05f;

	/*
	바닥·가구에 닿지 않은 점의 수평 속도 감쇠 (공기 저항)
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, ClampMax=1))
	float AirDrag = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Housekeeping|Blanket")
	FText GrabPrompt;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Blanket|Simulation", meta=(ClampMin=0, Units="cm"))
	float CollisionOffset = 2.0f;
	
private:
	struct FDistanceConstraint
	{
		int32 A;
		int32 B;
		float RestLength;
		float Stiffness;
	};
	int32 ToIndex(int32 X, int32 Y) const
	{
		return Y * (SegmentsX + 1) + X;
	}
	void InitParticles();
	void BuildConstraints();
	void Step(float Dt);
	void SolveConstraints();
	void SolveCollisions();
	/*
	잡은 점에서 원래 거리 이상 멀어진 점을 당겨옴 (늘어남 방지)
	*/
	void SolveTether();
	/*
	표면에서 밀어낼 때 경로에 다른 면이 있으면 두 면 사이 가운데에 둠 (좁은 틈 관통 방지)
	*/
	FVector ResolvePush(const FVector& SurfacePoint, const FVector& Normal, const FVector& Target,
		const FCollisionObjectQueryParams& ObjectParams, const FCollisionQueryParams& Params) const;
	void BuildTriangles();
	void BuildMesh(bool bCreate);
	void ComputeParticleNormals();
	TArray<FVector> Positions;
	TArray<FVector> PrevPositions;
	/*
	점별 바닥·가구 접촉 여부 (공기 저항 적용 판단)
	*/
	TArray<uint8> Contacts;
	TArray<FVector> ParticleNormals;
	TArray<FVector> SmoothedNormals;
	TArray<FDistanceConstraint> Constraints;
	TArray<FVector> MeshVertices;
	TArray<FVector> MeshNormals;
	TArray<FVector2D> MeshUVs;
	TArray<int32> MeshTriangles;
	int32 GrabbedIndex = INDEX_NONE;
	FVector GrabTarget = FVector::ZeroVector;
	float TimeAccumulator = 0.0f;
};