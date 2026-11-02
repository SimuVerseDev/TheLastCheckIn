(fn EnableReturnPoint ()
  (bind _homereturnzone (Variables|Default|GetHomeReturnZone))
  (Utilities|IsValid (Variables|Default|GetHomeReturnZone)
    (:"Is Valid"
      (Collision|SetCollisionEnabled _homereturnzone "QueryOnly")
      (Collision|SetCollisionResponseToAllChannels _homereturnzone)
      (Collision|SetCollisionResponseToChannel _homereturnzone "ECC_Visibility" "ECR_Block"))))

