(fn UpdateCartPickupPrompt ()
  (bind _cartpickupprompt (Variables|Default|GetCartPickupPrompt))
  (Utilities|IsValid _cartpickupprompt
    (:"Is Valid"
      (Widget|SetVisibility _cartpickupprompt "Collapsed")
      (Utilities|IsValid (Variables|Default|GetHeldCartObject)
        (:"Is Not Valid"
          (bind _target (CallFunction|FindCartInteraction))
          (bind _asbp_cart_cube (Utilities|Casting|CastToBP_CartCube _target))
          (bind _result (Class|BPCartPickupBase|CanPickUp _asbp_cart_cube))
          (if _result
            (Widget|SetVisibility (Variables|Default|GetCartPickupPrompt) "HitTestInvisible")))))))
