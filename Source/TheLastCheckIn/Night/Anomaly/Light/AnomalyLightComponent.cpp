// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/AnomalyLightComponent.h"
#include "TheLastCheckIn.h"

#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UAnomalyLightComponent::UAnomalyLightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAnomalyLightComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(Anomaly))
	{
		Anomaly = Cast<ALightAnomaly>(
			UGameplayStatics::GetActorOfClass(
				this, ALightAnomaly::StaticClass()));
	}

	if (!IsValid(Anomaly))
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[AnomalyLight] %s: LightAnomaly를 찾을 수 없습니다."),
			*GetNameSafe(GetOwner()));

		return;
	}

	Anomaly->OnLightingChanged.AddUniqueDynamic(
		this, &UAnomalyLightComponent::HandleLightingChanged);

	// 이상현상이 이미 진행 중일 때 생성된 조명도 현재 상태를 적용합니다.
	HandleLightingChanged(
		Anomaly->LightingMode,
		Anomaly->ActiveZoneId);
}

void UAnomalyLightComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Anomaly))
	{
		Anomaly->OnLightingChanged.RemoveDynamic(
			this, &UAnomalyLightComponent::HandleLightingChanged);
	}

	bFlickering = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlickerTimer);
	}

	RestoreOriginalState();

	Super::EndPlay(EndPlayReason);
}

void UAnomalyLightComponent::HandleLightingChanged(
	EHotelLightMode Mode,
	FName AffectedZoneId)
{
	bFlickering = false;
	GetWorld()->GetTimerManager().ClearTimer(FlickerTimer);

	const bool bThisZoneFlickers =
		Mode == EHotelLightMode::Flickering
		&& !ZoneId.IsNone()
		&& ZoneId == AffectedZoneId;

	const bool bBlackout =
		Mode == EHotelLightMode::Blackout;

	// 정상 상태 또는 깜빡임 대상이 아닌 구역
	if (!bThisZoneFlickers && !bBlackout)
	{
		RestoreOriginalState();
		return;
	}

	// 처음 영향을 받을 때만 저장합니다.
	// 깜빡임 → 정전 전환 시 원래 상태를 덮어쓰지 않습니다.
	CaptureOriginalState();

	if (bBlackout)
	{
		ApplyOutput(false);
		return;
	}

	bFlickering = true;
	bOutputOn = false;

	ApplyOutput(bOutputOn);
	ScheduleNextFlicker();
}

void UAnomalyLightComponent::CaptureOriginalState()
{
	if (bHasSnapshot)
	{
		return;
	}

	Lights.Reset();
	OriginalVisibility.Reset();

	TArray<ULightComponent*> FoundLights;
	GetOwner()->GetComponents<ULightComponent>(FoundLights);

	for (ULightComponent* Light : FoundLights)
	{
		if (!IsValid(Light))
		{
			continue;
		}

		Lights.Add(TWeakObjectPtr<ULightComponent>(Light));
		OriginalVisibility.Add(Light->IsVisible());
	}

	bHasSnapshot = true;

	if (Lights.IsEmpty())
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[AnomalyLight] %s: 같은 액터에 Light 컴포넌트가 없습니다."),
			*GetNameSafe(GetOwner()));
	}
}

void UAnomalyLightComponent::ApplyOutput(bool bOn)
{
	for (int32 Index = 0; Index < Lights.Num(); ++Index)
	{
		if (ULightComponent* Light = Lights[Index].Get())
		{
			// 원래 꺼져 있던 조명은 깜빡임 중에도 켜지지 않습니다.
			Light->SetVisibility(
				bOn && OriginalVisibility[Index]);
		}
	}

	// 필요하면 BP에서 전구의 발광 재질도 변경합니다.
	OnLightOutputChanged.Broadcast(bOn ? 1.0f : 0.0f);
}

void UAnomalyLightComponent::RestoreOriginalState()
{
	if (!bHasSnapshot)
	{
		return;
	}

	for (int32 Index = 0; Index < Lights.Num(); ++Index)
	{
		if (ULightComponent* Light = Lights[Index].Get())
		{
			Light->SetVisibility(OriginalVisibility[Index]);
		}
	}

	bHasSnapshot = false;
	bOutputOn = true;

	Lights.Reset();
	OriginalVisibility.Reset();

	OnLightOutputChanged.Broadcast(1.0f);
}

void UAnomalyLightComponent::ScheduleNextFlicker()
{
	if (!bFlickering)
	{
		return;
	}

	const float MinInterval = FMath::Max(0.02f, MinFlickerInterval);
	const float MaxInterval = FMath::Max(MinInterval, MaxFlickerInterval);
	const float Delay = FMath::FRandRange(MinInterval, MaxInterval);

	GetWorld()->GetTimerManager().SetTimer(
		FlickerTimer,
		this,
		&UAnomalyLightComponent::ToggleFlicker,
		Delay,
		false);
}

void UAnomalyLightComponent::ToggleFlicker()
{
	if (!bFlickering)
	{
		return;
	}

	bOutputOn = !bOutputOn;
	ApplyOutput(bOutputOn);

	ScheduleNextFlicker();
}

bool UAnomalyLightComponent::AssignToZone(
	AActor* ZoneOwner,
	ALightAnomaly* InAnomaly,
	FName InZoneId)
{
	if (!IsValid(ZoneOwner)
		|| !IsValid(InAnomaly)
		|| InZoneId.IsNone())
	{
		return false;
	}

	// 이미 다른 구역에서 관리 중인 조명은 가져오지 않습니다.
	if (AssignedZoneOwner.IsValid()
		&& AssignedZoneOwner.Get() != ZoneOwner)
	{
		return false;
	}

	// 기존 연결을 정리합니다.
	if (IsValid(Anomaly))
	{
		Anomaly->OnLightingChanged.RemoveDynamic(
			this, &UAnomalyLightComponent::HandleLightingChanged);
	}

	if (HasBegunPlay())
	{
		HandleLightingChanged(
			EHotelLightMode::Normal, NAME_None);
	}

	AssignedZoneOwner = ZoneOwner;
	Anomaly = InAnomaly;
	ZoneId = InZoneId;

	if (HasBegunPlay())
	{
		Anomaly->OnLightingChanged.AddUniqueDynamic(
			this, &UAnomalyLightComponent::HandleLightingChanged);

		// 이미 이상현상이 진행 중이면 현재 상태를 즉시 적용합니다.
		HandleLightingChanged(
			Anomaly->LightingMode,
			Anomaly->ActiveZoneId);
	}

	// 아직 BeginPlay 전이라면 기존 BeginPlay가 연결을 마무리합니다.
	return true;
}

void UAnomalyLightComponent::ReleaseFromZone(AActor* ZoneOwner)
{
	if (AssignedZoneOwner.Get() != ZoneOwner)
	{
		return;
	}

	if (IsValid(Anomaly))
	{
		Anomaly->OnLightingChanged.RemoveDynamic(
			this, &UAnomalyLightComponent::HandleLightingChanged);
	}

	if (HasBegunPlay())
	{
		HandleLightingChanged(
			EHotelLightMode::Normal, NAME_None);
	}

	AssignedZoneOwner.Reset();
	Anomaly = nullptr;
	ZoneId = NAME_None;
}
