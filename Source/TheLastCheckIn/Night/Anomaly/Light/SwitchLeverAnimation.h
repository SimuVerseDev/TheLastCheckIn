#pragma once

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

class FSwitchLeverAnimation
{
public:
	void Initialize(AActor& Owner, FName LeverMeshName)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(&Owner);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetFName() == LeverMeshName)
			{
				Mesh->SetMobility(EComponentMobility::Movable);
				const FQuat Up = Mesh->GetRelativeRotation().Quaternion();
				Levers.Add({Mesh, Up, Up});
			}
		}
	}

	void SetDown(AActor& Owner, bool bDown, const FRotator& DownRotation)
	{
		if (Levers.IsEmpty() || bDown == bIsDown)
		{
			return;
		}

		bIsDown = bDown;
		Elapsed = 0.f;
		RotationOffset = bDown ? DownRotation.Quaternion() : FQuat::Identity;
		for (FLever& Lever : Levers)
		{
			if (const UStaticMeshComponent* Mesh = Lever.Mesh.Get())
			{
				Lever.Start = Mesh->GetRelativeRotation().Quaternion();
			}
		}
		Owner.SetActorTickEnabled(true);
	}

	void Tick(AActor& Owner, float DeltaSeconds)
	{
		constexpr float TravelTime = 0.15f;
		Elapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(Elapsed / TravelTime, 0.f, 1.f);
		for (const FLever& Lever : Levers)
		{
			if (UStaticMeshComponent* Mesh = Lever.Mesh.Get())
			{
				Mesh->SetRelativeRotation(FQuat::Slerp(Lever.Start, Lever.Up * RotationOffset, Alpha));
			}
		}
		if (Alpha >= 1.f)
		{
			Owner.SetActorTickEnabled(false);
		}
	}

private:
	struct FLever
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		FQuat Up;
		FQuat Start;
	};

	TArray<FLever> Levers;
	FQuat RotationOffset = FQuat::Identity;
	float Elapsed = 0.f;
	bool bIsDown = false;
};
