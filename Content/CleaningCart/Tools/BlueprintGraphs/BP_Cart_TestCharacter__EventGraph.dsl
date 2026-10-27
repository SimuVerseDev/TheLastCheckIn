(event EventBeginPlay
  (bind _cartpickupprompt (Variables|Default|GetCartPickupPrompt))
  (|Parent:BeginPlay)
  (bind _returnvalue (UserInterface|CreateWidget "/Game/CleaningCart/Test/UI/WBP_HK_Crosshair.WBP_HK_Crosshair_C"))
  (UserInterface|Viewport|AddToViewport _returnvalue)
  (Development|PrintString "Crosshair OK")
  (bind _returnvalue_1 (UserInterface|CreateWidget "/Game/CleaningCart/Test/UI/WBP_CartPickupPrompt.WBP_CartPickupPrompt_C" (Game|GetPlayerController 0)))
  (Variables|Default|SetCartPickupPrompt _returnvalue_1)
  (Widget|SetVisibility _cartpickupprompt "Collapsed")
  (UserInterface|Viewport|AddToViewport _cartpickupprompt))

(event E (Key))

(event EventTick (DeltaSeconds)
  (CallFunction|CartUpdateCarry)
  (CallFunction|UpdateCartPickupPrompt))
