(fn CanPickUp ()
  (bind _result (CallFunction|IsinUse))
  (if _result
    (return false)
    (else
      (Utilities|IsValid (Variables|Default|GetHomeCart)
        (:"Is Valid"
          (bind _result_1 (CallFunction|IsinUse (Variables|Default|GetHomeCart)))
          (return (not _result_1)))
        (:"Is Not Valid"
          (return true))))))
