// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "LightZoneActor.generated.h"

class UBoxComponent;
class ALightAnomaly;
class UAnomalyLightComponent;

UCLASS()
class THELASTCHECKIN_API ALightZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ALightZoneActor();

	// Lobby, Floor2 등 스위치와 동일한 구역 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	FName ZoneId = NAME_None;

	// 비워 두면 레벨에서 LightAnomaly를 찾습니다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	TObjectPtr<ALightAnomaly> Anomaly;

	// 검색할 조명 종류. 처음에는 BP_HK_TableLamp_Lit를 추가합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Light")
	TArray<TSubclassOf<AActor>> TargetActorClasses;

	// 플레이 중 실제로 연결된 조명 액터 수
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Night|Light")
	int32 AssignedLightCount = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> ZoneBounds;

private:
	void AssignLights();

	bool MatchesTargetClass(const AActor* Actor) const;
	bool ContainsActor(const AActor* Actor) const;

	TArray<TWeakObjectPtr<UAnomalyLightComponent>> AssignedComponents;

	FTimerHandle AssignmentTimer;
};