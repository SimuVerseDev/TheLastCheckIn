#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "MirrorReflectionComponent.generated.h"

class UPrimitiveComponent;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

/*
거울 반사 전용 씬 캡처.
매 프레임 플레이어 카메라를 거울면 기준으로 뒤집은 위치에서 장면을 찍어 렌더 타깃에 담고,
거울 유리 머티리얼의 텍스처 파라미터로 넘겨준다.
머티리얼은 ScreenPosition UV를 좌우 반전해서 이 텍스처를 읽어야 한다.
*/
UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UMirrorReflectionComponent : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	UMirrorReflectionComponent();

	/*
	Plane: 위치 = 거울면 위의 한 점, 앞(X) 방향 = 거울이 바라보는 방향
	Surface: 반사 텍스처를 받을 유리 메시
	*/
	UFUNCTION(BlueprintCallable, Category="Mirror")
	void SetupMirror(USceneComponent* InPlane, UPrimitiveComponent* InSurface);

	UFUNCTION(BlueprintCallable, Category="Mirror")
	void SetReflectionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Mirror")
	bool IsReflectionEnabled() const { return bReflectionEnabled; }

	UFUNCTION(BlueprintPure, Category="Mirror")
	UMaterialInstanceDynamic* GetSurfaceMaterial() const { return SurfaceMID; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 화면 해상도 대비 반사 해상도 (1 = 화면과 같음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror|Quality", meta=(ClampMin="0.25", ClampMax="2.0"))
	float ResolutionScale = 1.0f;

	// 렌더 타깃 긴 변의 최대 픽셀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror|Quality", meta=(ClampMin="256", ClampMax="4096"))
	int32 MaxResolution = 2560;

	// 유리가 이 시간(초) 안에 화면에 그려지지 않았으면 캡처를 쉰다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mirror|Quality", meta=(ClampMin="0.0"))
	float VisibilityTimeout = 0.2f;

	// 유리 머티리얼에서 반사 텍스처를 받는 파라미터 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror")
	FName TextureParameterName = TEXT("ReflectionTexture");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror")
	int32 SurfaceMaterialIndex = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool UpdateCaptureTransform();
	void UpdateRenderTargetSize();
	bool IsSurfaceVisible() const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> MirrorPlane;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> MirrorSurface;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ReflectionTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	bool bReflectionEnabled = true;
};
