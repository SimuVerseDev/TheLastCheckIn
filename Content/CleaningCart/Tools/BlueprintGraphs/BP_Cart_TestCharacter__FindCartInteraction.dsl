(fn FindCartInteraction ()
  (bind _hitcomponent (Collision|BreakHitResult (Utilities|Array|ForEachLoop (Collision|MultiLineTraceByChannel (Camera|GetCameraLocation (Game|GetPlayerCameraManager 0)) (Math|Vector|vector+vector (Camera|GetCameraLocation (Game|GetPlayerCameraManager 0)) (Math|Vector|vector*vector (Math|Vector|GetForwardVector (Camera|GetCameraRotation (Game|GetPlayerCameraManager 0))) (Variables|Default|GetCartInteractDistance))) "TraceTypeQuery1" false (Utilities|Array|MakeArray (Variables|Default|GetHeldCartObject))))))
  (Variables|Default|SetFallbackCart 0)
  (Variables|Default|SetFallbackBody 0)
  (bind _outhits (Collision|MultiLineTraceByChannel (Camera|GetCameraLocation (Game|GetPlayerCameraManager 0)) (Math|Vector|vector+vector (Camera|GetCameraLocation (Game|GetPlayerCameraManager 0)) (Math|Vector|vector*vector (Math|Vector|GetForwardVector (Camera|GetCameraRotation (Game|GetPlayerCameraManager 0))) (Variables|Default|GetCartInteractDistance))) "TraceTypeQuery1" false (Utilities|Array|MakeArray (Variables|Default|GetHeldCartObject))))
  (for _array_element _outhits
    (bind _asbp_cart_pickup_base (Utilities|Casting|CastToBP_CartPickupBase (Collision|BreakHitResult _array_element)))
    (bind _result (Class|BPCartPickupBase|IsCartType _asbp_cart_pickup_base))
    (if _result
      (if (Components|ComponentHasTag _hitcomponent "CartReturnPoint")
        (return _asbp_cart_pickup_base _hitcomponent)
        (else
          (Utilities|IsValid (Variables|Default|GetFallbackCart)
            (:"Is Not Valid"
              (Variables|Default|SetFallbackCart _asbp_cart_pickup_base)
              (Variables|Default|SetFallbackBody _hitcomponent)))))
      (else
        (return _asbp_cart_pickup_base _hitcomponent))))
  (return (Variables|Default|GetFallbackCart) (Variables|Default|GetFallbackBody)))
