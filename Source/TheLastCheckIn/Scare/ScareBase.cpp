#include "Scare/ScareBase.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"

AScareBase::AScareBase()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AScareBase::Trigger()
{
	if (!bWaitForLook)
	{
		PlayScare();
		return;
	}
	OnArmed();
	GetWorldTimerManager().SetTimer(LookTimer, this, &AScareBase::CheckLook, LookCheckInterval, true);
}

void AScareBase::CheckLook()
{
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return;

	const FVector CamLoc = Cam->GetCameraLocation();
	const FVector Target = GetActorTransform().TransformPosition(LookTargetOffset);
	const FVector ToTarget = Target - CamLoc;
	const float Dist = ToTarget.Size();
	
	if (Dist > MaxLookDistance || Dist < KINDA_SMALL_NUMBER) return;
	
	if (FVector::DotProduct(Cam->GetCameraRotation().Vector(), ToTarget / Dist) < LookDotThreshold) return;
	
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ScareLook), false, this);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	if (GetWorld()->LineTraceTestByChannel(CamLoc, Target, ECC_Visibility, Params)) return;

	// 보였음 -> 같은 ID 나머지 해제 후 나만 실행
	for (TActorIterator<AScareBase> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->ScareId == ScareId)
		{
			It->Disarm();
		}
	}
	GetWorldTimerManager().ClearTimer(LookTimer);
	PlayScare();
}

void AScareBase::Disarm()
{
	if (!GetWorldTimerManager().IsTimerActive(LookTimer)) return;
	GetWorldTimerManager().ClearTimer(LookTimer);
	OnDisarmed();
}