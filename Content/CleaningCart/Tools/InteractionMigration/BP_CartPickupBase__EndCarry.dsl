(fn EndCarry ()
  (Variables|Default|SetCarrier 0)
  (Physics|Components|PhysicsConstraint|BreakConstraint (Variables|Default|GetHomeCartCollisionFilter))
  (Collision|SetCollisionResponseToChannel (Variables|Default|GetPickupBody) "ECC_Pawn" "ECR_Block"))

