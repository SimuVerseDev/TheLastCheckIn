(fn CartUpdateCarry ()
  (bind _heldcartobject (Variables|Default|GetHeldCartObject))
  (Utilities|IsValid (Variables|Default|GetHeldCartObject)
    (:"Is Valid"
      (if (> (Transformation|GetDistanceTo _heldcartobject) 450.0)
        (CallFunction|CartRelease)
        (else
          (bind _result (Class|BPCartPickupBase|IsCartType _heldcartobject))
          (Physics|Components|PhysicsHandle|SetTargetLocationAndRotation (Variables|Default|GetCartGrabHandle) (select _result (Math|Vector|vector+vector (Math|Vector|vector+vector (Transformation|GetActorLocation) (Math|Vector|vector*vector (Math|Vector|GetForwardVector (Math|Rotator|MakeRotator 0.0 0.0 (.yaw (Camera|GetCameraRotation (Game|GetPlayerCameraManager 0))))) 160.0)) (Math|Vector|MakeVector 0.0 0.0 -30.0)) (Math|Vector|vector+vector (Math|Vector|vector+vector (Camera|GetCameraLocation (Game|GetPlayerCameraManager 0)) (Math|Vector|vector*vector (Math|Vector|GetForwardVector (Camera|GetCameraRotation (Game|GetPlayerCameraManager 0))) 120.0)) (Math|Vector|MakeVector 0.0 0.0 -15.0))) (Math|Rotator|MakeRotator 0.0 0.0 (.yaw (Camera|GetCameraRotation (Game|GetPlayerCameraManager 0))))))))))
