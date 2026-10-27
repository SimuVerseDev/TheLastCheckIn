(fn InitializeCartSlot (NewCart Slot ReturnZone)
  (Variables|Default|SetHomeReturnZone ReturnZone)
  (Class|ActorComponent|SetComponentTags (Utilities|Array|MakeArray (Utilities|Name|MakeLiteralName "CartReturnPoint")) ReturnZone)
  (CallFunction|StoreonCart NewCart Slot))
