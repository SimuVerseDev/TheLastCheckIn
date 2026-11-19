// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/LightZoneActor.h"

#include "Night/Anomaly/Light/LightAnomaly.h"
#include "Night/Anomaly/Light/AnomalyLightComponent.h"
#include "TheLastCheckIn.h"
#include "Components/BoxComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ALightZoneActor::ALightZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(
		TEXT("ZoneBounds"));

	SetRootComponent(ZoneBounds);

	ZoneBounds->InitBoxExtent(FVector(500.0f, 500.0f, 200.0f));

	// 물리 충돌이나 Overlap 이벤트 없이 위치로 판정합니다.
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneBounds->SetGenerateOverlapEvents(false);
	ZoneBounds->SetCanEverAffectNavigation(false);
	ZoneBounds->SetHiddenInGame(true);
}

void ALightZoneActor::BeginPlay()
{
	Super::BeginPlay();

	// 다른 액터들의 초기화 이후에 조명을 연결합니다.
	AssignmentTimer = GetWorldTimerManager().SetTimerForNextTick(
		this, &ALightZoneActor::AssignLights);
}

void ALightZoneActor::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AssignmentTimer);

	for (const TWeakObjectPtr<UAnomalyLightComponent>& WeakComponent
		: AssignedComponents)
	{
		if (UAnomalyLightComponent* Component = WeakComponent.Get())
		{
			Component->ReleaseFromZone(this);
		}
	}

	AssignedComponents.Reset();
	AssignedLightCount = 0;

	Super::EndPlay(EndPlayReason);
}

bool ALightZoneActor::MatchesTargetClass(const AActor* Actor) const
{
	for (const TSubclassOf<AActor>& TargetClass : TargetActorClasses)
	{
		if (TargetClass.Get() && Actor->IsA(TargetClass.Get()))
		{
			return true;
		}
	}

	return false;
}

bool ALightZoneActor::ContainsActor(const AActor* Actor) const
{
	// 박스의 회전과 스케일까지 반영하여 로컬 좌표로 검사합니다.
	const FVector LocalPosition =
		ZoneBounds->GetComponentTransform().InverseTransformPosition(
			Actor->GetActorLocation());

	const FVector Extent = ZoneBounds->GetUnscaledBoxExtent();

	return FMath::Abs(LocalPosition.X) <= Extent.X
		&& FMath::Abs(LocalPosition.Y) <= Extent.Y
		&& FMath::Abs(LocalPosition.Z) <= Extent.Z;
}

void ALightZoneActor::AssignLights()
{
	if (ZoneId.IsNone())
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[LightZone] %s: ZoneId를 설정해 주세요."),
			*GetName());

		return;
	}

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
			TEXT("[LightZone] %s: LightAnomaly를 찾을 수 없습니다."),
			*GetName());

		return;
	}

	if (TargetActorClasses.IsEmpty())
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[LightZone] %s: TargetActorClasses를 설정해 주세요."),
			*GetName());

		return;
	}

	AssignedLightCount = 0;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;

		if (!IsValid(Actor)
			|| !MatchesTargetClass(Actor)
			|| !ContainsActor(Actor))
		{
			continue;
		}

		// PointLight, SpotLight, RectLight 등의 실제 빛이 있는지 확인합니다.
		if (!Actor->FindComponentByClass<ULocalLightComponent>())
		{
			continue;
		}

		TArray<UAnomalyLightComponent*> ExistingComponents;
		Actor->GetComponents<UAnomalyLightComponent>(ExistingComponents);

		// 조명 하나를 여러 컴포넌트가 동시에 제어하지 않도록 합니다.
		if (ExistingComponents.Num() > 1)
		{
			UE_LOG(
				LogAnomaly, Warning,
				TEXT("[LightZone] %s: AnomalyLightComponent가 여러 개여서 제외했습니다."),
				*Actor->GetName());

			continue;
		}

		UAnomalyLightComponent* Component =
			ExistingComponents.IsEmpty()
				? nullptr
				: ExistingComponents[0];

		const bool bCreateComponent = Component == nullptr;

		if (bCreateComponent)
		{
			// 플레이 중인 액터에만 추가되는 임시 컴포넌트입니다.
			Component = NewObject<UAnomalyLightComponent>(
				Actor, NAME_None, RF_Transient);
		}

		if (!Component)
		{
			continue;
		}

		if (!Component->AssignToZone(this, Anomaly, ZoneId))
		{
			UE_LOG(
				LogAnomaly, Warning,
				TEXT("[LightZone] %s: %s가 다른 구역에 연결되어 있습니다. 박스 겹침을 확인하세요."),
				*GetName(),
				*Actor->GetName());

			if (bCreateComponent)
			{
				Component->DestroyComponent();
			}

			continue;
		}

		if (bCreateComponent)
		{
			Actor->AddInstanceComponent(Component);
			Component->RegisterComponent();
		}

		AssignedComponents.Add(
			TWeakObjectPtr<UAnomalyLightComponent>(Component));

		++AssignedLightCount;
	}

	UE_LOG(
		LogAnomaly, Log,
		TEXT("[LightZone] %s: ZoneId=%s, 연결된 조명 액터=%d"),
		*GetName(),
		*ZoneId.ToString(),
		AssignedLightCount);

	if (AssignedLightCount == 0)
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[LightZone] %s: 연결된 조명이 없습니다. 박스 위치와 대상 클래스를 확인하세요."),
			*GetName());
	}
}
