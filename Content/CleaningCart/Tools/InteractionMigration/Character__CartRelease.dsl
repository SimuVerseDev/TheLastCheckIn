(fn CartRelease ()
  (Physics|Components|PhysicsHandle|ReleaseComponent (Variables|Default|GetCartGrabHandle))
  (Utilities|IsValid (Variables|Default|GetHeldCartObject)
    (:"Is Valid"
      (Class|BPCartPickupBase|EndCarry (Variables|Default|GetHeldCartObject))
      (Variables|Default|SetHeldCartObject 0))
    (:"Is Not Valid"
      (Variables|Default|SetHeldCartObject 0))))

