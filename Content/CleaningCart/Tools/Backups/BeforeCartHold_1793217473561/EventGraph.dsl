(event EventBlueprintInitializeAnimation
  (bind _ascharacter (Utilities|Casting|CastToCharacter (Animation|GetOwningActor)))
  (bind _output_get (Variables|Default|SetCharacter _ascharacter))
  (Variables|Default|SetMovementComponent (Class|Character|GetCharacterMovement _output_get)))

(event EventBlueprintUpdateAnimation (DeltaTimeX)
  (ToolMenus|Get)
  (bind _output_get (Variables|Default|SetVelocity (Variables|Default|GetVelocity (Variables|Default|GetMovementComponent))))
  (Variables|Default|SetGroundSpeed (Math|Vector|VectorLengthXY _output_get))
  (Variables|Default|SetDirection (select (|GetbOrientRotationToMovement (Variables|Default|GetMovementComponent)) (Math|Float|Clamp(Float) (Animation|CalculateDirection (Variables|Default|GetVelocity) (Transformation|GetActorRotation (Variables|Default|GetCharacter))) -45.000000 45.000000) (Animation|CalculateDirection (Variables|Default|GetVelocity) (Transformation|GetActorRotation (Variables|Default|GetCharacter)))))
  (Variables|Default|SetShouldMove (and (> (Variables|Default|GetGroundSpeed) 0.010000) (Math|Vector|NotEqual(Vector) (Pawn|Components|CharacterMovement|GetCurrentAcceleration (Variables|Default|GetMovementComponent)) 0 0)))
  (Variables|Default|SetIsFalling (AI|Components|NavMovement|IsFalling (Variables|Default|GetMovementComponent))))
