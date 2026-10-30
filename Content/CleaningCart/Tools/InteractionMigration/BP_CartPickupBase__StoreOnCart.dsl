(fn StoreOnCart (NewCart Slot)
  (bind _pickupbody (Variables|Default|GetPickupBody))
  (Variables|Default|SetHomeCart NewCart)
  (Variables|Default|SetHomeSlot Slot)
  (Variables|Default|SetCarrier 0)
  (Physics|SetSimulatePhysics _pickupbody)
  (Physics|Components|PhysicsConstraint|BreakConstraint (Variables|Default|GetHomeCartCollisionFilter))
  (Collision|SetCollisionEnabled _pickupbody "QueryOnly")
  (Collision|SetCollisionResponseToAllChannels _pickupbody)
  (Collision|SetCollisionResponseToChannel _pickupbody "ECC_Visibility" "ECR_Block")
  (bind _returnvalue (Transformation|AttachActorToComponent Slot "None" "SnapToTarget" "SnapToTarget" "KeepWorld" false))
  (Variables|Default|SetIsStored _returnvalue)
  (if _returnvalue
    (Utilities|IsValid (Variables|Default|GetHomeReturnZone)
      (:"Is Valid"
        (Collision|SetCollisionEnabled (Variables|Default|GetHomeReturnZone))))))

