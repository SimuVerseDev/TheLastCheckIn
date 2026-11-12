#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "ShadowFigureActor.generated.h"

class UStaticMeshComponent;
class AShadowFigureActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShadowFigureAbsorbed, AShadowFigureActor*, Figure);

UCLASS()
class THELASTCHECKIN_API AShadowFigureActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AShadowFigureActor();

	UFUNCTION(BlueprintCallable, Category="Night|Shadow")
	void Appear();

	UFUNCTION(BlueprintCallable, Category="Night|Shadow")
	void Disappear();

	UFUNCTION(BlueprintPure, Category="Night|Shadow")
	bool IsPresent() const
	{
		return bPresent;
	}

	UPROPERTY(BlueprintAssignable, Category="Night|Shadow")
	FShadowFigureAbsorbed OnAbsorbed;

	/*
	Interactable
	*/
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool CanUse_Implementation(AActor* Interactor) const override;
	virtual FText GetUsePrompt_Implementation(AActor* Interactor) const override;
	virtual bool Use_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Night|Shadow")
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UPROPERTY(EditAnywhere, Category="Night|Shadow", meta=(ClampMin="0.05", Units="s"))
	float AbsorbDuration = 0.8f;

	UPROPERTY(EditAnywhere, Category="Night|Shadow")
	FText AbsorbPrompt;

private:
	FTransform HomeTransform;
	TWeakObjectPtr<USceneComponent> AbsorbTarget;
	FVector AbsorbStartLocation = FVector::ZeroVector;
	FVector AbsorbStartScale = FVector::OneVector;
	float AbsorbElapsed = 0.0f;
	bool bPresent = false;
	bool bAbsorbing = false;
};