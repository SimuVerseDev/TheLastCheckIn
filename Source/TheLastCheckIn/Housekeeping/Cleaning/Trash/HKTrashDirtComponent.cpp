#include "Housekeeping/Cleaning/Trash/HKTrashDirtComponent.h"
#include "Housekeeping/Cleaning/Trash/HKTrashActor.h"
#include "Components/BoxComponent.h"

UHKTrashDirtComponent::UHKTrashDirtComponent()
{
	Tolerance = 0.0f;
	bRandomCandidate = false;
}

/*
Virtual
*/
float UHKTrashDirtComponent::EvaluateDirt() const
{
	const AHKTrashActor* Trash = Cast<AHKTrashActor>(GetOwner());
	/*
	Already absorbed
	*/
	if (Trash && Trash->IsAbsorbed())
	{
		return 0.0f;
	}
	/*
	Box : room boundary
	*/
	const UBoxComponent* Box = RoomBounds.Get();
	if (!Box)
	{
		return 1.0f;
	}
	
	const FVector Extent = Box->GetUnscaledBoxExtent();
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(GetOwner()->GetActorLocation());
	const bool bInside = FMath::Abs(Local.X) <= Extent.X && 
					 FMath::Abs(Local.Y) <= Extent.Y && 
					 FMath::Abs(Local.Z) <= Extent.Z;
	
	return bInside ? 1.0f : 0.0f;
}
