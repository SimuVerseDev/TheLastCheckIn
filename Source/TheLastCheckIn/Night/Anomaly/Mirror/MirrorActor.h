#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MirrorActor.generated.h"

class UStaticMeshComponent;
class UMirrorReflectionComponent;

UENUM(BlueprintType)
enum class EMirrorState : uint8
{
	Intact,
	Cracked,
	Shattered
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FMirrorStateChanged,
	AMirrorActor*, Mirror,
	EMirrorState, OldState,
	EMirrorState, NewState);

/*
반사되는 거울. 프레임 + 유리 + 반사 캡처를 한 액터로 묶고 깨짐 상태를 관리한다.
MirrorPlane은 유리 표면 위에 두고 앞(X) 방향이 거울이 바라보는 쪽을 향하게 맞춘다.
파편, 사운드 같은 연출은 OnCracked / OnShattered에서 블루프린트로 붙인다.
*/
UCLASS()
class THELASTCHECKIN_API AMirrorActor : public AActor
{
	GENERATED_BODY()

public:
	AMirrorActor();

	// 금 감 - 반사는 유지, 유리 머티리얼 CrackParameterName을 1로
	UFUNCTION(BlueprintCallable, Category="Mirror")
	bool Crack();

	// 산산조각 - 유리를 숨기고 반사를 끈다
	UFUNCTION(BlueprintCallable, Category="Mirror")
	bool Shatter();

	// 원래대로 (이상현상 해결 등)
	UFUNCTION(BlueprintCallable, Category="Mirror")
	bool Repair();

	UFUNCTION(BlueprintPure, Category="Mirror")
	EMirrorState GetMirrorState() const { return MirrorState; }

	UPROPERTY(BlueprintAssignable, Category="Mirror")
	FMirrorStateChanged OnMirrorStateChanged;

	// 유리 머티리얼에서 금 표현을 켜는 스칼라 파라미터 (0 = 멀쩡, 1 = 금 감)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror")
	FName CrackParameterName = TEXT("CrackAmount");

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Mirror")
	void OnCracked();
	UFUNCTION(BlueprintImplementableEvent, Category="Mirror")
	void OnShattered();
	UFUNCTION(BlueprintImplementableEvent, Category="Mirror")
	void OnRepaired();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror")
	TObjectPtr<UStaticMeshComponent> GlassMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror")
	TObjectPtr<USceneComponent> MirrorPlane;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror")
	TObjectPtr<UMirrorReflectionComponent> Reflection;

private:
	void SetMirrorState(EMirrorState NewState);
	void SetCrackAmount(float Amount) const;

	EMirrorState MirrorState = EMirrorState::Intact;
};
