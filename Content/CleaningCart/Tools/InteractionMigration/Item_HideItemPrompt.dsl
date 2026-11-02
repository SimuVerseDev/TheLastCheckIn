(fn HideItemPrompt ()
  (Utilities|IsValid (Variables|Default|GetItemPickupPrompt)
    (:"Is Valid"
      (Widget|RemovefromParent (Variables|Default|GetItemPickupPrompt))
      (Variables|Default|SetItemPickupPrompt 0))))
