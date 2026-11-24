#pragma once

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

class FSwitchLeverAnimation
{
public:
	void Initialize(AActor& Owner, FName LeverMeshName)
	{
		Levers.Reset();
		TInlineComponentArray<UStaticMeshComponent*> Meshes(&Owner);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetFName() == LeverMeshName)
			{
				Mesh->SetMobility(EComponentMobility::Movable);
				const FQuat Up = Mesh->GetRelativeRotation().Quaternion();
				Levers.Add({Mesh, Up, Up, Up});
			}
		}
		Levers.Sort([](const FLever& A, const FLever& B) { return A.Mesh->GetName() < B.Mesh->GetName(); });
	}

	TArray<UStaticMeshComponent*> GetMeshes() const
	{
		TArray<UStaticMeshComponent*> Result;
		for (const FLever& Lever : Levers) if (Lever.Mesh.IsValid()) Result.Add(Lever.Mesh.Get());
		return Result;
	}

	bool IsDown(UStaticMeshComponent* Mesh) const
	{
		for (const FLever& Lever : Levers) if (Lever.Mesh.Get() == Mesh) return Lever.bDown;
		return false;
	}

	void SetLeverDown(AActor& Owner, UStaticMeshComponent* Mesh, bool bDown, const FRotator& DownRotation)
	{
		for (FLever& Lever : Levers)
		{
			if (Lever.Mesh.Get() != Mesh || Lever.bDown == bDown) continue;
			Lever.bDown = bDown;
			Lever.Start = Mesh->GetRelativeRotation().Quaternion();
			Lever.Target = Lever.Up * (bDown ? DownRotation.Quaternion() : FQuat::Identity);
			Lever.Elapsed = 0.f;
			Lever.bAnimating = true;
			Owner.SetActorTickEnabled(true);
		}
	}

	void SetDown(AActor& Owner, bool bDown, const FRotator& DownRotation)
	{
		for (const FLever& Lever : Levers)
		{
			if (UStaticMeshComponent* Mesh = Lever.Mesh.Get()) SetLeverDown(Owner, Mesh, bDown, DownRotation);
		}
	}

	void Tick(AActor& Owner, float DeltaSeconds)
	{
		constexpr float TravelTime = 0.15f;
		bool bAnyAnimating = false;
		for (FLever& Lever : Levers)
		{
			if (!Lever.bAnimating) continue;
			Lever.Elapsed += DeltaSeconds;
			const float Alpha = FMath::Clamp(Lever.Elapsed / TravelTime, 0.f, 1.f);
			if (UStaticMeshComponent* Mesh = Lever.Mesh.Get())
			{
				Mesh->SetRelativeRotation(FQuat::Slerp(Lever.Start, Lever.Target, Alpha));
			}
			Lever.bAnimating = Alpha < 1.f && Lever.Mesh.IsValid();
			bAnyAnimating |= Lever.bAnimating;
		}
		Owner.SetActorTickEnabled(bAnyAnimating);
	}

private:
	struct FLever
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		FQuat Up;
		FQuat Start;
		FQuat Target;
		float Elapsed = 0.f;
		bool bDown = false;
		bool bAnimating = false;
	};

	TArray<FLever> Levers;
};
