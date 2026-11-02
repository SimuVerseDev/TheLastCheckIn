(fn ItemUpdateCarry ()
  (Utilities|IsValid (Variables|Default|GetCarrier)
    (:"Is Valid"
      (Utilities|IsValid (Variables|Default|GetItemController)
        (:"Is Valid"
          (if (> (Transformation|GetDistanceTo :OtherActor (Variables|Default|GetCarrier)) 450.0)
            (CallFunction|ItemRelease)
            (else
              (bind (viewLocation viewRotation) (Pawn|GetPlayerViewPoint (Variables|Default|GetItemController)))
              (bind carryLocation (+ (+ viewLocation (* (Math|Vector|GetForwardVector viewRotation) 120.0)) (Math|Vector|MakeVector 0.0 0.0 -15.0)))
              (bind carryRotation (Math|Rotator|MakeRotator :Yaw (.yaw viewRotation)))
              (bind mop (Utilities|Casting|CastToBP_CartMop self)
                (:then
                  (bind character (Utilities|Casting|CastToTheLastCheckInCharacter (Variables|Default|GetCarrier))
                    (:then
                      (bind hand (Transformation|GetSocketTransform (Class|TheLastCheckinCharacter|GetFirstPersonMesh character) "hand_r" "RTS_World"))
                      (bind pose (Class|BPCartMop|GetHandCarryTransform mop hand))
                      (Transformation|SetActorTransform :NewTransform pose :bTeleport true))
                    (:CastFailed
                      (Transformation|SetActorTransform :NewTransform (Math|Transform|MakeTransform carryLocation carryRotation) :bTeleport true))))
                (:CastFailed
                  (Physics|Components|PhysicsHandle|SetTargetLocationAndRotation (Variables|Default|GetItemGrabHandle) carryLocation carryRotation))))))
        (:"Is Not Valid" (CallFunction|ItemRelease))))
    (:"Is Not Valid" (CallFunction|ItemRelease))))
