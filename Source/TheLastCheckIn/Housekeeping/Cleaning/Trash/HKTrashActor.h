#pragma once
#include "CoreMinimal.h"
#include "Housekeeping/Carry/PickupActor/HKPhysicsPickupActor.h"
#include "HKTrashActor.generated.h"
class UHKTrashDirtComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EHKTrashState : uint8
{
	Active,		// 레벨에 놓여 있음
	Absorbing,	// 카트로 빨려 들어가는 중
	Pooled		// 흡수 후 재사용 대기
};

UCLASS()
class AHKTrashActor : public AHKPhysicsPickupActor
{
	GENERATED_BODY()
	
public:
	AHKTrashActor();
	virtual void Tick(float DeltaTime) override;
	
	bool Absorb(USceneComponent* Target);
	
	void EnterPool();
	void ExitPool(const FTransform& SpawnTransform);
	
	UFUNCTION(BlueprintPure, Category="Housekeeping|Trash")
	EHKTrashState GetTrashState() const
	{
		return State;
	}
	UFUNCTION(BlueprintPure, Category="Housekeeping|Trash")
	bool IsAbsorbed() const
	{
		return State != EHKTrashState::Active;
	}
	UFUNCTION(BlueprintPure, Category="Housekeeping|Trash")
	bool IsPooled() const
	{
		return State == EHKTrashState::Pooled;
	}
	
	/*
	Design
	*/
	void SetHighlighted(bool bHighlighted);
	
	UHKTrashDirtComponent* GetDirtComponent() const
	{
		return Dirt;
	}
	FText GetAbsorbPrompt() const
	{
		return AbsorbPrompt;
	}

	/*
	HKInteractable
	*/
	virtual bool CanUse_Implementation(AActor* Interactor) const override;
	virtual FText GetUsePrompt_Implementation(AActor* Interactor) const override;
	virtual bool Use_Implementation(AActor* Interactor) override;
	virtual void OnInteractFocusBegin_Implementation(AActor* Interactor) override;
	virtual void OnInteractFocusEnd_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UHKTrashDirtComponent> Dirt;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Housekeeping|Trash")
	FText AbsorbPrompt;
	
	UPROPERTY(EditAnywhere, Category="Housekeeping|Trash", meta=(ClampMin=0.05, Units="s"))
	float AbsorbDuration = 0.35f;
	
	/*
	Design
	*/
	UPROPERTY(EditAnywhere, Category="Housekeeping|Trash")
	TObjectPtr<UMaterialInterface> HighlightMaterial;
	
	UFUNCTION(BlueprintImplementableEvent, Category="Housekeeping|Trash", meta=(DisplayName="On Absorbed"))
	void BP_OnAbsorbed();
	
private:
	EHKTrashState State = EHKTrashState::Active;
	TWeakObjectPtr<USceneComponent> AbsorbTarget;
	FVector AbsorbStartLocation = FVector::ZeroVector;
	FVector OriginalScale = FVector::OneVector;
	float AbsorbElapsed = 0.0f;
};