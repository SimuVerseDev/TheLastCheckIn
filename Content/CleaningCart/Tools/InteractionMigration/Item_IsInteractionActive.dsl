(fn IsInteractionActive (Interactor)
  (return (and (Variables|Default|GetItemCarrying) (== (Variables|Default|GetCarrier) Interactor))))
