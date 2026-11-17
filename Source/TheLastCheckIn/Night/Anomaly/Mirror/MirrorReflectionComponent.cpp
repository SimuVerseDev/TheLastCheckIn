#include "Night/Anomaly/Mirror/MirrorReflectionComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

UMirrorReflectionComponent::UMirrorReflectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 카메라 매니저 갱신(UpdateCameraManager)이 끝난 뒤에 따라가야 한 프레임 늦지 않는다
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	// 직접 CaptureScene()을 부른다
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;
	// Lumen, 시간 누적 효과가 매 프레임 이어지도록 렌더링 상태 유지
	bAlwaysPersistRenderingState = true;

	// 후처리 전 HDR 색 - 유리 머티리얼이 Emissive로 내보내면 본 화면 톤매핑이 한 번만 적용됨
	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

	// 거울 뒤쪽은 찍지 않음 (r.AllowGlobalClipPlane=True 필요)
	bEnableClipPlane = true;

	// 위치/회전은 매 프레임 월드 기준으로 직접 지정
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UMirrorReflectionComponent::SetupMirror(USceneComponent* InPlane, UPrimitiveComponent* InSurface)
{
	MirrorPlane = InPlane;
	MirrorSurface = InSurface;

	ClearHiddenComponents();
	if (MirrorSurface)
	{
		// 유리 자신이 반사에 찍히면 거울면을 가린다
		HideComponent(MirrorSurface);
	}

	if (HasBegunPlay() && MirrorSurface)
	{
		SurfaceMID = MirrorSurface->CreateAndSetMaterialInstanceDynamic(SurfaceMaterialIndex);
		if (SurfaceMID && ReflectionTarget)
		{
			SurfaceMID->SetTextureParameterValue(TextureParameterName, ReflectionTarget);
		}
	}
}

void UMirrorReflectionComponent::SetReflectionEnabled(bool bEnabled)
{
	bReflectionEnabled = bEnabled;
	SetComponentTickEnabled(bEnabled);
}

void UMirrorReflectionComponent::BeginPlay()
{
	Super::BeginPlay();

	ReflectionTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("MirrorReflectionTarget"));
	ReflectionTarget->RenderTargetFormat = RTF_RGBA16f;
	ReflectionTarget->ClearColor = FLinearColor::Black;
	ReflectionTarget->bAutoGenerateMips = false;
	ReflectionTarget->InitAutoFormat(256, 256);
	ReflectionTarget->UpdateResourceImmediate(true);
	TextureTarget = ReflectionTarget;

	if (MirrorSurface)
	{
		SurfaceMID = MirrorSurface->CreateAndSetMaterialInstanceDynamic(SurfaceMaterialIndex);
		if (SurfaceMID)
		{
			SurfaceMID->SetTextureParameterValue(TextureParameterName, ReflectionTarget);
		}
	}

	SetComponentTickEnabled(bReflectionEnabled);
}

void UMirrorReflectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TextureTarget = nullptr;
	if (ReflectionTarget)
	{
		ReflectionTarget->ReleaseResource();
		ReflectionTarget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UMirrorReflectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bReflectionEnabled || !ReflectionTarget || !MirrorPlane || !IsSurfaceVisible())
	{
		return;
	}
	if (!UpdateCaptureTransform())
	{
		return;
	}
	UpdateRenderTargetSize();
	CaptureScene();
}

bool UMirrorReflectionComponent::IsSurfaceVisible() const
{
	if (!MirrorSurface || !MirrorSurface->IsVisible())
	{
		return false;
	}
	return VisibilityTimeout <= 0.0f || MirrorSurface->WasRecentlyRendered(VisibilityTimeout);
}

bool UMirrorReflectionComponent::UpdateCaptureTransform()
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return false;
	}

	const FMinimalViewInfo& View = CameraManager->GetCameraCacheView();
	const FVector PlanePoint = MirrorPlane->GetComponentLocation();
	const FVector PlaneNormal = MirrorPlane->GetForwardVector();

	// 거울 뒤에서는 반사가 보이지 않는다
	const double Distance = FVector::DotProduct(View.Location - PlanePoint, PlaneNormal);
	if (Distance <= 0.0)
	{
		return false;
	}

	const FRotationMatrix CameraAxes(View.Rotation);
	const FVector Forward = CameraAxes.GetUnitAxis(EAxis::X);
	const FVector Up = CameraAxes.GetUnitAxis(EAxis::Z);

	// 위치와 방향 벡터를 거울면 기준으로 뒤집음 - 결과 이미지는 좌우가 반대라 머티리얼에서 U를 뒤집어 읽는다
	const FVector MirroredLocation = View.Location - 2.0 * Distance * PlaneNormal;
	const FVector MirroredForward = Forward - 2.0 * FVector::DotProduct(Forward, PlaneNormal) * PlaneNormal;
	const FVector MirroredUp = Up - 2.0 * FVector::DotProduct(Up, PlaneNormal) * PlaneNormal;

	SetWorldLocationAndRotation(MirroredLocation, FRotationMatrix::MakeFromXZ(MirroredForward, MirroredUp).Rotator());

	FOVAngle = View.FOV;
	ClipPlaneBase = PlanePoint;
	ClipPlaneNormal = PlaneNormal;
	return true;
}

void UMirrorReflectionComponent::UpdateRenderTargetSize()
{
	FVector2D ViewportSize(1280.0, 720.0);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	if (ViewportSize.X < 1.0 || ViewportSize.Y < 1.0)
	{
		return;
	}

	// 화면과 같은 비율이어야 ScreenPosition으로 읽었을 때 어긋나지 않는다
	double Width = ViewportSize.X * ResolutionScale;
	double Height = ViewportSize.Y * ResolutionScale;
	const double LongSide = FMath::Max(Width, Height);
	if (LongSide > MaxResolution)
	{
		const double Fit = MaxResolution / LongSide;
		Width *= Fit;
		Height *= Fit;
	}

	const int32 SizeX = FMath::Max(1, FMath::RoundToInt32(Width));
	const int32 SizeY = FMath::Max(1, FMath::RoundToInt32(Height));
	if (ReflectionTarget->SizeX != SizeX || ReflectionTarget->SizeY != SizeY)
	{
		ReflectionTarget->ResizeTarget(SizeX, SizeY);
	}
}
