(fn ShowItemPrompt (Interactor)
  (CallFunction|HideItemPrompt)
  (if (not (Variables|Default|GetIsCart))
    (bind pawn (Utilities|Casting|CastToPawn Interactor)
      (:then
        (bind pc (Utilities|Casting|CastToPlayerController (Pawn|GetController pawn))
          (:then
            (bind widget (UserInterface|CreateWidget :Class "/Game/CleaningCart/Test/UI/WBP_CartPickupPrompt.WBP_CartPickupPrompt_C" :OwningPlayer pc))
            (Variables|Default|SetItemPickupPrompt widget)
            (UserInterface|Viewport|AddtoPlayerScreen widget)
            (Widget|SetVisibility widget "HitTestInvisible")))))))
