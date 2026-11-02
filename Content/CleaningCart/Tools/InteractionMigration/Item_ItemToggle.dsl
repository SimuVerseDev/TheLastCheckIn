(fn ItemToggle (Interactor)
  (if (Variables|Default|GetItemCarrying)
    (if (== (Variables|Default|GetCarrier) Interactor)
      (bind returned (CallFunction|TryReturnItem))
      (if (not returned)
        (CallFunction|ItemRelease)))
    (else
      (bind available (CallFunction|CanPickUp))
      (if (and available (not (Variables|Default|GetIsCart)))
        (bind pawn (Utilities|Casting|CastToPawn Interactor)
          (:then
            (bind pc (Utilities|Casting|CastToPlayerController (Pawn|GetController pawn))
              (:then
                (Variables|Default|SetItemController pc)
                (CallFunction|BeginCarry :NewCarrier Interactor)
                (Variables|Default|SetItemCarrying true)
                (bind mop (Utilities|Casting|CastToBP_CartMop self)
                  (:then
                    (Physics|SetSimulatePhysics (Variables|Default|GetPickupBody) false)
                    (Collision|SetCollisionEnabled (Variables|Default|GetPickupBody) "QueryOnly")
                    (CallFunction|ItemUpdateCarry))
                  (:CastFailed
                    (Physics|Components|PhysicsHandle|GrabComponentAtLocationWithRotation
                      (Variables|Default|GetItemGrabHandle)
                      (Variables|Default|GetPickupBody) "None"
                      (Transformation|GetWorldLocation (Variables|Default|GetPickupBody))
                      (Transformation|GetActorRotation))
                    (CallFunction|ItemUpdateCarry)))))))))))
