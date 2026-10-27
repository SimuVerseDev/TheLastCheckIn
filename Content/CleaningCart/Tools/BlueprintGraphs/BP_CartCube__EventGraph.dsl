(event EventBeginPlay
  (|Parent:BeginPlay))

(event Collision|EventActorBeginOverlap (OtherActor)
  (Collision|Parent:ActorBeginOverlap OtherActor))

(event EventTick (DeltaSeconds)
  (|Parent:Tick DeltaSeconds))
