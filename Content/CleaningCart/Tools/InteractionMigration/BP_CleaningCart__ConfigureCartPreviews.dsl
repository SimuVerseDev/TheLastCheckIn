(fn ConfigureCartPreviews ()
  (bind _moppreview (Variables|Default|GetMopPreview))
  (bind _clothpreview (Variables|Default|GetClothPreview))
  (bind _trashbinpreview (Variables|Default|GetTrashBinPreview))
  (bind _cartappearance (Variables|Default|GetCartAppearance))
  (bind _childactor (Class|ChildActorComponent|GetChildActor _cartappearance))
  (Collision|SetCollisionEnabled _moppreview)
  (Development|SetHiddenInGame _moppreview true)
  (Collision|SetCollisionEnabled _clothpreview)
  (Development|SetHiddenInGame _clothpreview true)
  (Collision|SetCollisionEnabled _trashbinpreview)
  (Development|SetHiddenInGame _trashbinpreview true)
  (Utilities|IsValid _childactor
    (:"Is Valid"
      (Collision|SetActorEnableCollision _childactor))))

