#include "Shop/ShopPlacedProductComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Housekeeping/Core/Carry/HKPhysicsPickupActor.h"

void UShopPlacedProductComponent::InitializeProduct(FName InProductId, UStaticMesh* InPreviewMesh)
{
	ProductId = InProductId;
	PreviewMesh = InPreviewMesh;
}

bool UShopPlacedProductComponent::CanRelocate() const
{
	const AActor* Actor = GetOwner();
	if (bRelocating || !IsValid(Actor) || Actor->IsActorBeingDestroyed() || !PreviewMesh ||
		!Actor->GetRootComponent() || Actor->IsHidden() || Actor->GetAttachParentActor() || Actor->GetOwner()) return false;
	if (const auto* Pickup = Cast<AHKPhysicsPickupActor>(Actor)) if (Pickup->IsCarried()) return false;
	return true;
}

bool UShopPlacedProductComponent::BeginRelocation()
{
	if (!CanRelocate()) return false;
	AActor* Actor = GetOwner();
	OriginalTransform = Actor->GetActorTransform();
	bOriginalHidden = Actor->IsHidden();
	bOriginalCollision = Actor->GetActorEnableCollision();
	bOriginalTick = Actor->IsActorTickEnabled();
	SceneStates.Reset();
	TickingComponents.Reset();
	// Snapshot every component before changing mobility or physics (which can detach components).
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component->IsComponentTickEnabled()) TickingComponents.Add(Component);
		if (auto* Scene = Cast<USceneComponent>(Component))
		{
			FSceneState& State = SceneStates.AddDefaulted_GetRef();
			State.Component = Scene;
			State.Parent = Scene->GetAttachParent();
			State.Socket = Scene->GetAttachSocketName();
			State.WorldTransform = Scene->GetComponentTransform();
			State.Mobility = Scene->Mobility;
			for (auto* Parent = Scene->GetAttachParent(); Parent; Parent = Parent->GetAttachParent()) ++State.AttachmentDepth;
			if (auto* Primitive = Cast<UPrimitiveComponent>(Scene))
			{
				State.bSimulating = Primitive->IsSimulatingPhysics();
				State.LinearVelocity = Primitive->GetPhysicsLinearVelocity();
				State.AngularVelocity = Primitive->GetPhysicsAngularVelocityInDegrees();
			}
		}
	}
	SceneStates.Sort([](const FSceneState& A, const FSceneState& B) { return A.AttachmentDepth < B.AttachmentDepth; });
	bRelocating = true;
	for (const auto& State : SceneStates)
	{
		if (auto* Scene = State.Component.Get())
		{
			if (auto* Primitive = Cast<UPrimitiveComponent>(Scene)) if (State.bSimulating) Primitive->SetSimulatePhysics(false);
			Scene->SetMobility(EComponentMobility::Movable);
		}
	}
	for (const auto& Component : TickingComponents) if (Component.IsValid()) Component->SetComponentTickEnabled(false);
	Actor->SetActorTickEnabled(false);
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	return true;
}

bool UShopPlacedProductComponent::FinishRelocation(const FTransform& Destination)
{
	if (!bRelocating || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) return false;
	if (Destination.ContainsNaN() || !Destination.IsValid())
	{
		CancelRelocation();
		return false;
	}
	FTransform FinalTransform = Destination;
	FinalTransform.SetScale3D(OriginalTransform.GetScale3D());
	Restore(FinalTransform, false);
	return true;
}

void UShopPlacedProductComponent::CancelRelocation()
{
	if (bRelocating && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()) Restore(OriginalTransform, true);
}

void UShopPlacedProductComponent::Restore(const FTransform& Destination, bool bCancelled)
{
	AActor* Actor = GetOwner();
	// Reuse the original instance, including lights, custom variables, and detached physics pieces.
	for (const auto& State : SceneStates)
		if (auto* Scene = State.Component.Get())
		{
			if (State.Parent.IsValid()) Scene->AttachToComponent(State.Parent.Get(), FAttachmentTransformRules::KeepWorldTransform, State.Socket);
			else Scene->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	Actor->SetActorTransform(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	for (const auto& State : SceneStates)
		if (auto* Scene = State.Component.Get())
			Scene->SetWorldTransform(bCancelled ? State.WorldTransform : State.WorldTransform.GetRelativeTransform(OriginalTransform) * Destination,
				false, nullptr, ETeleportType::TeleportPhysics);
	Actor->SetActorEnableCollision(bOriginalCollision);
	for (const auto& State : SceneStates)
		if (auto* Scene = State.Component.Get())
		{
			Scene->SetMobility(State.Mobility);
			if (auto* Primitive = Cast<UPrimitiveComponent>(Scene)) if (State.bSimulating)
			{
				Primitive->SetSimulatePhysics(true);
				Primitive->SetPhysicsLinearVelocity(bCancelled ? State.LinearVelocity : FVector::ZeroVector);
				Primitive->SetPhysicsAngularVelocityInDegrees(bCancelled ? State.AngularVelocity : FVector::ZeroVector);
			}
		}
	Actor->SetActorHiddenInGame(bOriginalHidden);
	Actor->SetActorTickEnabled(bOriginalTick);
	for (const auto& Component : TickingComponents) if (Component.IsValid()) Component->SetComponentTickEnabled(true);
	SceneStates.Reset();
	TickingComponents.Reset();
	bRelocating = false;
}

void UShopPlacedProductComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelRelocation();
	Super::EndPlay(Reason);
}
