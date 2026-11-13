#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interaction/Interactable.h"
#include "ShadowFigureActor.generated.h"

class UStaticMeshComponent;
class UMeshComponent;
class UMaterialInterface;
class AShadowFigureActor;
class AHKVacuumToolActor;

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

	UPROPERTY(EditAnywhere, Category="Night|Shadow|Absorb", meta=(ClampMin="0.05", Units="s"))
	float AbsorbDuration = 1.3f;

	// 흡입 초반 제자리에서 버티는 구간 비율 - 나머지 구간은 끌려감
	UPROPERTY(EditAnywhere, Category="Night|Shadow|Absorb", meta=(ClampMin="0", ClampMax="0.9"))
	float ResistRatio = 0.35f;

	// 끌려가는 동안 흡입구 방향으로 늘어나는 최대 배율
	UPROPERTY(EditAnywhere, Category="Night|Shadow|Absorb", meta=(ClampMin="1"))
	float StretchScale = 2.6f;

	// 버티는 동안 떨리는 폭
	UPROPERTY(EditAnywhere, Category="Night|Shadow|Absorb", meta=(ClampMin="0", Units="cm"))
	float ResistShake = 6.0f;

	// 연기 머티리얼의 지직거림 파라미터
	UPROPERTY(EditAnywhere, Category="Night|Shadow|Absorb")
	FName GlitchParameter = TEXT("Glitch");

	UPROPERTY(EditAnywhere, Category="Night|Shadow")
	FText AbsorbPrompt;

	// 맨눈으로는 안 보이고 CCTV 화면에서만 보임 (흡입 조준은 가능)
	UPROPERTY(EditAnywhere, Category="Night|Shadow")
	bool bCCTVOnly = false;

private:
	void UpdateCCTVVisibility();
	bool IsViewedOnCCTV() const;
	void TickAbsorb(float DeltaSeconds);
	void SetCardsHidden(bool bHideCards);
	void SetGlitch(float Value);
	void RestoreMaterials();

	FTransform HomeTransform;
	TWeakObjectPtr<AHKVacuumToolActor> AbsorbVacuum;
	FVector AbsorbStartLocation = FVector::ZeroVector;
	FQuat AbsorbStartRotation = FQuat::Identity;
	FVector AbsorbStartScale = FVector::OneVector;
	float AbsorbElapsed = 0.0f;
	bool bPresent = false;
	bool bAbsorbing = false;

	// 흡입 중 지직거림 값을 바꾼 머티리얼을 다음 등장 때 되돌리기 위한 원본 (에셋이라 GC 대상 아님)
	TMap<TWeakObjectPtr<UMeshComponent>, TArray<UMaterialInterface*>> OriginalMaterials;
};
