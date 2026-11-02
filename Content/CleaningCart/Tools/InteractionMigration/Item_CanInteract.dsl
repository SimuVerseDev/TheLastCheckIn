(fn CanInteract (Interactor)
  (if (Variables|Default|GetIsCart)
    (return false)
    (elif (Variables|Default|GetItemCarrying)
      (return (== (Variables|Default|GetCarrier) Interactor))
      (else
        (Utilities|IsValid (Variables|Default|GetCarrier)
          (:"Is Valid" (return false))
          (:"Is Not Valid"
            (Utilities|IsValid (Variables|Default|GetHomeCart)
              (:"Is Valid"
                (bind busy (CallFunction|IsinUse :self (Variables|Default|GetHomeCart)))
                (return (not busy)))
              (:"Is Not Valid" (return true)))))))))
