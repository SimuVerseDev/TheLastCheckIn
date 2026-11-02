(fn ConstructionScript ()
  (bind _visualmesh (Variables|Default|GetVisualMesh))
  (Components|Box|SetBoxExtent (Variables|Default|GetPickupBody) (Variables|Default|GetBodyHalfExtent))
  (Components|StaticMesh|SetStaticMesh _visualmesh (Variables|Default|GetVisualAsset))
  (Transformation|SetRelativeScale3D _visualmesh (Variables|Default|GetVisualScale))
  (Transformation|SetRelativeLocation _visualmesh (Variables|Default|GetVisualOffset)))

