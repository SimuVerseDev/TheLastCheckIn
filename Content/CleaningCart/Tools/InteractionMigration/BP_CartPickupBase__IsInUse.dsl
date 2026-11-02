(fn IsInUse ()
  (Utilities|IsValid (Variables|Default|GetCarrier)
    (:"Is Valid"
      (return true))
    (:"Is Not Valid"
      (return false))))

