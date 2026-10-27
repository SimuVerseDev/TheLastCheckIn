(fn CartInteract ()
  (Utilities|IsValid (Variables|Default|GetHeldCartObject)
    (:"Is Valid"
      (bind _handled (CallFunction|TryReturntoCart))
      (if (not _handled)
        (CallFunction|CartRelease)))
    (:"Is Not Valid"
      (bind (_target _hitcomponent) (CallFunction|FindCartInteraction))
      (Utilities|IsValid _target
        (:"Is Valid"
          (if (not (Components|ComponentHasTag _hitcomponent "CartReturnPoint"))
            (CallFunction|GrabCartObject _target)))))))
