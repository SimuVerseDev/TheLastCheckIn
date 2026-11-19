// Fill out your copyright notice in the Description page of Project Settings.


#include "Night/Anomaly/Light/LightAnomaly.h"

#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "TheLastCheckIn.h"
#include "Night/Anomaly/AnomalyManagerSubsystem.h"
#include "Phone/HotelPhoneActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ALightAnomaly::ALightAnomaly()
{
	// 매 프레임 검사 대신 게임 시간 변경 알림을 사용합니다.
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(
		CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AnomalyId = FName(TEXT("Light"));
	DisplayName = NSLOCTEXT(
		"LightAnomaly", "Name", "조명 이상현상");

	EarliestStartHour = 18;
	LatestStartHour = 3;

	// 예시 구역입니다. BP에서 실제 호텔 구역에 맞게 수정하세요.
	FlickerZoneIds.Add(FName(TEXT("Lobby")));
	FlickerZoneIds.Add(FName(TEXT("Floor1")));
	FlickerZoneIds.Add(FName(TEXT("StaffRoom")));

	BlackoutComplaint = NSLOCTEXT(
		"LightAnomaly",
		"BlackoutComplaint",
		"호텔 조명이 전부 꺼졌어요. 빨리 확인해 주세요!");

	RestoredComplaint = NSLOCTEXT(
		"LightAnomaly",
		"RestoredComplaint",
		"아까 갑자기 불이 꺼져서 놀랐어요. 지금은 들어오지만 확인 부탁드려요.");
}

void ALightAnomaly::BeginPlay()
{
	Super::BeginPlay();

	Clock = GetWorld()->GetSubsystem<UGameClockSubsystem>();

	if (Clock)
	{
		Clock->OnTimeAdvanced.AddUniqueDynamic(
			this, &ALightAnomaly::HandleTimeAdvanced);
	}

	// Phone은 부모 AnomalyBase에서 지정하거나 자동으로 찾습니다.
	if (IsValid(Phone))
	{
		Phone->OnCallAnswered.AddUObject(
			this, &ALightAnomaly::HandleComplaintAnswered);

		Phone->OnCallFinished.AddUObject(
			this, &ALightAnomaly::HandleComplaintFinished);
	}

	if (UAnomalyManagerSubsystem* Manager =
		GetWorld()->GetSubsystem<UAnomalyManagerSubsystem>())
	{
		Manager->RegisterAnomaly(this);
	}
}

void ALightAnomaly::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (Clock)
	{
		Clock->OnTimeAdvanced.RemoveDynamic(
			this, &ALightAnomaly::HandleTimeAdvanced);
	}

	bComplaintPending = false;

	if (IsValid(Phone))
	{
		Phone->OnCallAnswered.RemoveAll(this);
		Phone->OnCallFinished.RemoveAll(this);

		Phone->StopRingingFor(this);
	}

	SetLightingMode(EHotelLightMode::Normal);

	Super::EndPlay(EndPlayReason);
}

bool ALightAnomaly::StartAnomaly()
{
	// 이전 이상현상이나 전화가 남아 있으면 새로 시작하지 않습니다.
	if (!Clock
		|| IsRunning()
		|| bComplaintPending
		|| (IsValid(Phone) && Phone->HasCallFor(this)))
	{
		return false;
	}

	// 18:00 이상 ~ 다음 날 05:00 미만에만 시작합니다.
	const int32 Hour = Clock->GetHour();

	if (Hour >= 3 && Hour < 18)
	{
		return false;
	}

	// 비어 있는 구역 ID와 중복을 제외합니다.
	TArray<FName> Candidates;

	for (const FName ZoneId : FlickerZoneIds)
	{
		if (!ZoneId.IsNone())
		{
			Candidates.AddUnique(ZoneId);
		}
	}

	if (Candidates.IsEmpty())
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[LightAnomaly] FlickerZoneIds를 설정해 주세요."));

		return false;
	}

	if (!IsValid(Phone))
	{
		UE_LOG(
			LogAnomaly, Warning,
			TEXT("[LightAnomaly] 레벨에 배치된 Phone을 지정해 주세요."));

		return false;
	}

	// 전체 발생 중 20%는 즉시 정전으로 시작합니다.
	bStartedWithBlackout =
		FMath::FRand() <
		FMath::Clamp(ImmediateBlackoutChance, 0.0f, 1.0f);

	ActiveZoneId =
		Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	ComplaintDueSeconds = 0.0;
	
	// 새 발생에 대한 수신 기록 초기화
	bComplaintAnswered = false;
	bComplaintFinished = false;

	if (!Super::StartAnomaly())
	{
		return false;
	}

	if (bStartedWithBlackout)
	{
		// Active를 거쳐 즉시 Escalated(정전)로 전환합니다.
		return Escalate();
	}

	SetLightingMode(EHotelLightMode::Flickering);
	return true;
}

bool ALightAnomaly::Escalate()
{
	if (!Clock || GetState() != EAnomalyState::Active)
	{
		return false;
	}

	if (!Super::Escalate())
	{
		return false;
	}

	// 정전 시점부터 게임 시간 5분 뒤에 전화합니다.
	ComplaintDueSeconds =
		Clock->GetTotalGameSeconds()
		+ GameTime::MinutesToSeconds(FMath::Max(1, ComplaintDelayMinutes));

	bComplaintPending = true;

	SetLightingMode(EHotelLightMode::Blackout);
	return true;
}

bool ALightAnomaly::TryResolveFromSwitch(FName SwitchZoneId)
{
	// 정전 중이거나 다른 구역의 스위치라면 해결할 수 없습니다.
	if (GetState() != EAnomalyState::Active
		|| LightingMode != EHotelLightMode::Flickering
		|| SwitchZoneId != ActiveZoneId)
	{
		return false;
	}

	return Resolve();
}

bool ALightAnomaly::TryRestoreFromBreaker()
{
	// 차단기는 전체 정전 상태에서만 동작합니다.
	if (GetState() != EAnomalyState::Escalated
		|| LightingMode != EHotelLightMode::Blackout)
	{
		return false;
	}

	return Resolve();
}

bool ALightAnomaly::Resolve()
{
	if (!IsRunning())
	{
		return false;
	}

	// 처음부터 정전으로 시작했고,
	// 전화 예정 시각 전에 복구했다면 전화가 필요 없습니다.
	const bool bCancelComplaint =
		LightingMode == EHotelLightMode::Blackout
		&& bStartedWithBlackout
		&& bComplaintPending
		&& Clock
		&& Clock->GetTotalGameSeconds() < ComplaintDueSeconds;

	if (!Super::Resolve())
	{
		return false;
	}

	if (bCancelComplaint)
	{
		bComplaintPending = false;

		if (IsValid(Phone))
		{
			Phone->StopRingingFor(this);
		}
	}

	SetLightingMode(EHotelLightMode::Normal);

	if (!bCancelComplaint)
	{
		// 아직 받지 않은 필수 전화는 유지하면서 대사만 갱신합니다.
		RefreshComplaintMessage();
	}

	return true;
}

bool ALightAnomaly::Fail()
{
	if (!Super::Fail())
	{
		return false;
	}

	SetLightingMode(EHotelLightMode::Normal);

	// 조명이 복원되었으므로 미수신 전화의 대사도 맞춰줍니다.
	RefreshComplaintMessage();

	// 자동 복원은 플레이어의 조기 해결로 취급하지 않습니다.
	// 남아 있는 전화 예약은 그대로 유지합니다.
	return true;
}

void ALightAnomaly::HandleTimeAdvanced(
	double PreviousSeconds,
	double CurrentSeconds)
{
	if (!bComplaintPending
		|| CurrentSeconds < ComplaintDueSeconds
		|| !IsValid(Phone))
	{
		return;
	}

	const FText& Message =
		LightingMode == EHotelLightMode::Blackout
			? BlackoutComplaint
			: RestoredComplaint;

	// 전화기가 사용 중이어도 대기열에 접수할 수 있습니다.
	// 접수에 성공했을 때만 예약 상태를 해제합니다.
	if (Phone->RequestCallFor(this, Message))
	{
		bComplaintPending = false;
	}
}

void ALightAnomaly::SetLightingMode(EHotelLightMode NewMode)
{
	LightingMode = NewMode;

	if (NewMode == EHotelLightMode::Normal)
	{
		ActiveZoneId = NAME_None;
	}

	OnLightingChanged.Broadcast(LightingMode, ActiveZoneId);
}

void ALightAnomaly::RefreshComplaintMessage()
{
	// 이미 받은 전화는 대사를 변경하지 않습니다.
	// 아직 접수하지 않은 예약은 예정 시각에 대사를 선택합니다.
	if (bComplaintAnswered
		|| !IsValid(Phone)
		|| !Phone->HasCallFor(this))
	{
		return;
	}

	const FText& Message =
		LightingMode == EHotelLightMode::Blackout
			? BlackoutComplaint
			: RestoredComplaint;

	// 같은 요청자의 미수신 전화는 새로 추가되지 않고
	// 대기 중이거나 울리는 전화의 대사만 갱신됩니다.
	Phone->RequestCallFor(this, Message);
}

void ALightAnomaly::HandleComplaintAnswered(
	const UObject* Requester)
{
	// 다른 이상현상의 전화 알림은 무시합니다.
	if (Requester != this)
	{
		return;
	}

	bComplaintAnswered = true;
	bComplaintPending = false;
}

void ALightAnomaly::HandleComplaintFinished(
	const UObject* Requester)
{
	if (Requester != this)
	{
		return;
	}

	bComplaintFinished = true;
}

