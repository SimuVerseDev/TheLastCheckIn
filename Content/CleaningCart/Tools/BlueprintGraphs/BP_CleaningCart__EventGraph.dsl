(event EventBeginPlay
  (bind _pickupbody (Variables|Default|GetPickupBody))
  (Collision|SetCollisionProfileName _pickupbody "PhysicsActor")
  (Collision|SetCollisionResponseToChannel _pickupbody "ECC_Visibility" "ECR_Overlap")
  (Physics|SetMassOverrideInKg _pickupbody "None" 30.0)
  (Physics|SetSimulatePhysics _pickupbody true)
  (CallFunction|SpawnCartItems))

(event Collision|EventActorBeginOverlap (OtherActor)
  (Collision|Parent:ActorBeginOverlap OtherActor))

(event EventTick (DeltaSeconds)
  (|Parent:Tick DeltaSeconds))
