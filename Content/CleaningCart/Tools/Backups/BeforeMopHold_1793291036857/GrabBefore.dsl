(fn GrabCartObject (Target)
  (bind _result (Class|BPCartPickupBase|CanPickUp Target))
  (if _result
    (Class|BPCartPickupBase|BeginCarry Target self)
    (Variables|Default|SetHeldCartObject Target)
    (bind _body (Class|BPCartPickupBase|GetPickupBodyComponent Target))
    (Physics|Components|PhysicsHandle|GrabComponentAtLocationWithRotation (Variables|Default|GetCartGrabHandle) _body "None" (Transformation|GetWorldLocation _body) (Transformation|GetActorRotation Target))
    (CallFunction|CartUpdateCarry)
    (else
      (Development|PrintString "카트를 먼저 내려놓아 주세요" true true "(R=0.000000,G=0.660000,B=1.000000,A=1.000000)" 1.5))))
