(fn TryReturnItem ()
  (Variables|Default|SetItemReturnHandled false)
  (bind (viewLocation viewRotation) (Pawn|GetPlayerViewPoint (Variables|Default|GetItemController)))
  (bind hits (Collision|MultiLineTraceByChannel
    :Start viewLocation
    :End (+ viewLocation (* (Math|Vector|GetForwardVector viewRotation) 250.0))
    :TraceChannel "TraceTypeQuery1"
    :ActorsToIgnore (Utilities|Array|MakeArray (Variables|Default|GetCarrier))
    :bIgnoreSelf true))
  (for hit hits
    (bind (blocking initial time distance location impact normal impactNormal material actor component) (Collision|BreakHitResult hit))
    (if (Components|ComponentHasTag component "CartReturnPoint")
      (Variables|Default|SetItemReturnHandled true)
      (if (== component (Variables|Default|GetHomeReturnZone))
        (Utilities|IsValid (Variables|Default|GetHomeCart)
          (:"Is Valid"
            (bind busy (CallFunction|IsinUse :self (Variables|Default|GetHomeCart)))
            (if (not busy)
              (Utilities|IsValid (Variables|Default|GetHomeSlot)
                (:"Is Valid"
                  (CallFunction|ItemRelease)
                  (CallFunction|StoreonCart :NewCart (Variables|Default|GetHomeCart) :Slot (Variables|Default|GetHomeSlot)))))))
        (else
          (Development|PrintString "이 물품의 원래 자리를 조준해 주세요")))))
  (return (Variables|Default|GetItemReturnHandled)))
