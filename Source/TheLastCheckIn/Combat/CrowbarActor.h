#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "CrowbarActor.generated.h"

class UStaticMeshComponent;
class UCrowbarCombatComponent;

UCLASS()
class THELASTCHECKIN_API ACrowbarActor
	: public AActor
	, public IInteractable
{
	GENERATED_BODY()

	friend class UCrowbarCombatComponent;

public:
	ACrowbarActor();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	virtual FText GetInteractPrompt_Implementation(AActor* Interactor) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;

	virtual bool IsInteractionActive_Implementation(AActor* Interactor) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowbar")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 손에 붙일 소켓
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowbar")
	FName AttachSocket = NAME_None;

	// 장착 위치 보정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowbar")
	FTransform HandTransform = FTransform::Identity;

private:
	UCrowbarCombatComponent* FindCombat(AActor* Actor) const;
};