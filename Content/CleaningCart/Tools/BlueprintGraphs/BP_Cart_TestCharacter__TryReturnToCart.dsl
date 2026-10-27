(fn TryReturnToCart ()
  (bind _heldcartobject (Variables|Default|GetHeldCartObject))
  (bind _homecart (Class|BPCartPickupBase|GetHomeCart _heldcartobject))
  (bind _result (Class|BPCartPickupBase|IsCartType _heldcartobject))
  (if _result
    (return false)
    (else
      (bind _hitcomponent (CallFunction|FindCartInteraction))
      (Utilities|IsValid _hitcomponent
        (:"Is Valid"
          (if (Components|ComponentHasTag _hitcomponent "CartReturnPoint")
            (if (Utilities|Equal(Object) _hitcomponent (Class|BPCartPickupBase|GetHomeReturnZone _heldcartobject))
              (Utilities|IsValid (Class|BPCartPickupBase|GetHomeCart _heldcartobject)
                (:"Is Valid"
                  (bind _result_1 (Class|BPCartPickupBase|IsinUse _homecart))
                  (if _result_1
                    (return true)
                    (else
                      (Utilities|IsValid (Class|BPCartPickupBase|GetHomeSlot _heldcartobject)
                        (:"Is Valid"
                          (Physics|Components|PhysicsHandle|ReleaseComponent (Variables|Default|GetCartGrabHandle))
                          (Class|BPCartPickupBase|EndCarry _heldcartobject)
                          (Class|BPCartPickupBase|StoreonCart _heldcartobject _homecart (Class|BPCartPickupBase|GetHomeSlot _heldcartobject))
                          (if (Class|BPCartPickupBase|GetIsStored _heldcartobject)
                            (Variables|Default|SetHeldCartObject 0)
                            (return true)
                            (else
                              (Variables|Default|SetHeldCartObject 0)
                              (return true))))
                        (:"Is Not Valid"
                          (return true))))))
                (:"Is Not Valid"
                  (return true)))
              (else
                (Development|PrintString "이 물품의 원래 자리를 조준해 주세요" true true "(R=0.000000,G=0.660000,B=1.000000,A=1.000000)" 1.5)
                (return true)))
            (else
              (return false))))
        (:"Is Not Valid"
          (return false))))))
