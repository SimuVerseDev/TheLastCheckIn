(fn ApplyCartPreview (Preview)
  (bind _returnvalue (Transformation|GetRelativeTransform Preview))
  (bind _visualmesh (Variables|Default|GetVisualMesh))
  (bind _staticmesh (Class|StaticMeshComponent|GetStaticMesh Preview))
  (Utilities|IsValid Preview
    (:"Is Valid"
      (Variables|Default|SetVisualAsset _staticmesh)
      (Variables|Default|SetVisualScale (.scale _returnvalue))
      (Variables|Default|SetVisualOffset (.location _returnvalue))
      (Components|StaticMesh|SetStaticMesh _visualmesh _staticmesh)
      (Transformation|SetRelativeTransform _visualmesh _returnvalue false true))))

