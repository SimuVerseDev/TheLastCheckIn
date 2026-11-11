#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ReputationHUDSubsystem.generated.h"

class UReputationHUDWidget;

// Shows the reputation HUD on the local player's screen without touching the shared PlayerController.
UCLASS()
class THELASTCHECKIN_API UReputationHUDSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Game|Reputation")
	void SetHUDVisible(bool bVisible);

private:
	void CreateHUD();

	UPROPERTY() TObjectPtr<UReputationHUDWidget> HUDWidget;
};
