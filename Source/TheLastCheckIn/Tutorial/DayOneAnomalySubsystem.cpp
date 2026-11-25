#include "Tutorial/DayOneAnomalySubsystem.h"
#include "Core/Time/GameClockSubsystem.h"
#include "Core/Time/GameTimeMath.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CCTV/CCTVMonitor.h"
#include "Night/Anomaly/Shadow/ShadowFigureActor.h"
#include "Night/Anomaly/Knock/KnockDoorActor.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "Math/RotationMatrix.h"
#include "Memo/MemoSubsystem.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	constexpr EDayOneAnomaly Presentations[] = {
		EDayOneAnomaly::LobbyFlicker, EDayOneAnomaly::ShadowFigure, EDayOneAnomaly::DoorKnock
	};
}

void UDayOneAnomalySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMemoSubsystem>();
	Clock = Collection.InitializeDependency<UGameClockSubsystem>();
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	Clock->OnTimeAdvanced.AddDynamic(this, &ThisClass::HandleTimeAdvanced);
	Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
	Phase->OnPhaseChanged.AddDynamic(this, &ThisClass::HandlePhaseChanged);
}

void UDayOneAnomalySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CCTVViewingTimer);
	}
	
	StopSchedule();
	StopLobbyFlicker();
	StopShadowPresentation();
	StopDoorKnock();
	
	ShadowCCTVMonitor.Reset();
	ShadowCandidates.Reset();

	if (Clock) Clock->OnTimeAdvanced.RemoveAll(this);
	if (Phase)
	{
		Phase->OnDayStarted.RemoveAll(this);
		Phase->OnPhaseChanged.RemoveAll(this);
	}
	OnPresentationStarted.Clear();
	OnPresentationEnded.Clear();
	Progress.Reset();
	Super::Deinitialize();
}

FDayOneAnomalyProgress UDayOneAnomalySubsystem::GetProgress(EDayOneAnomaly Presentation) const
{
	const auto* Entry = Progress.Find(Presentation);
	return Entry ? *Entry : FDayOneAnomalyProgress();
}

void UDayOneAnomalySubsystem::HandleDayStarted(int32 Day)
{
	if (Day != 1) { StopSchedule(); return; }
	// Duplicate day notifications must not reroll or replay a presentation.
	if (bDayOneScheduled) return;
	bDayOneScheduled = true;
	bScheduleActive = true;
	auto AddWindow = [this](EDayOneAnomaly Id, double Start, double End)
	{
		auto& Entry = Progress.Add(Id);
		Entry.WindowStartSeconds = Start;
		Entry.WindowEndSeconds = End;
	};
	// 최초 시작 예정 시각 이후, 최대 18:30까지 로비 진입을 기다립니다.
	AddWindow(
		EDayOneAnomaly::LobbyFlicker,
		GameTime::HoursToSeconds(18),
		GameTime::HoursToSeconds(18.5));
	AddWindow(EDayOneAnomaly::ShadowFigure, GameTime::HoursToSeconds(20), GameTime::HoursToSeconds(21.5));
	AddWindow(EDayOneAnomaly::DoorKnock, GameTime::HoursToSeconds(22), GameTime::HoursToSeconds(23.5));
	PlannedFlickerStartSeconds = GameTime::HoursToSeconds(18) + FMath::RandRange(0, 300);
	Evaluate(Clock->GetTotalGameSeconds());
}

void UDayOneAnomalySubsystem::HandleTimeAdvanced(double PreviousSeconds, double CurrentSeconds)
{
	// Rewinding the debug clock must not replay a finished presentation.
	if (CurrentSeconds >= PreviousSeconds) Evaluate(CurrentSeconds);
}

void UDayOneAnomalySubsystem::HandlePhaseChanged(EHotelPhase OldPhase, EHotelPhase NewPhase)
{
	if (NewPhase == EHotelPhase::Settlement) StopSchedule();
}

void UDayOneAnomalySubsystem::SetCCTVViewing(bool bViewing)
{
	bViewingCCTV = bViewing;

	if (Clock)
	{
		Evaluate(Clock->GetTotalGameSeconds());
	}

	UpdateShadowVisibility();
	CheckShadowWitness();
}

void UDayOneAnomalySubsystem::SetPlayerInStaffRoom(bool bInside)
{
	bInStaffRoom = bInside;
	if (Clock) Evaluate(Clock->GetTotalGameSeconds());
}

void UDayOneAnomalySubsystem::Evaluate(double Now)
{
	if (!bScheduleActive)
	{
		return;
	}

	for (EDayOneAnomaly Id : Presentations)
	{
		if (!bScheduleActive)
		{
			break;
		}

		auto& Entry = Progress.FindChecked(Id);
		// 노크는 다른 연출과 달리 23:30 이후에도 미발생분을 실행합니다.
		// 종료는 게임 시각이 아닌 10초짜리 월드 타이머가 담당합니다.
		if (Id == EDayOneAnomaly::DoorKnock)
		{
			if (Entry.State == EDayOneAnomalyState::Waiting
				&& Now >= Entry.WindowStartSeconds)
			{
				const bool bPreferredContext =
					bViewingCCTV || bInStaffRoom;

				const bool bFallbackTime =
					Now >= Entry.WindowEndSeconds;

				if (bPreferredContext || bFallbackTime)
				{
					// -1: 게임 시간 기준 종료 시각을 사용하지 않음.
					StartPresentation(Id, Now, -1.0);
				}
			}

			continue;
		}
		const bool bFlicker = Id == EDayOneAnomaly::LobbyFlicker;

		const bool bCondition = bFlicker
			? bInLobby
			: (Id == EDayOneAnomaly::ShadowFigure
				? bViewingCCTV
				: bViewingCCTV || bInStaffRoom);

		if (Entry.State == EDayOneAnomalyState::Active)
		{
			// 조명은 로비를 떠나도 계속 진행합니다.
			// 형체는 CCTV를 끄면 종료합니다.
			// 노크는 위 전용 분기와 별도 타이머에서 처리합니다.
			const bool bContextEnded = !bFlicker && !bCondition;

			if (Now >= Entry.EndAtSeconds || bContextEnded)
			{
				EndPresentation(Id);
			}

			continue;
		}

		if (Entry.State != EDayOneAnomalyState::Waiting)
		{
			continue;
		}

		// 조명은 정확히 18:30까지 진입을 허용합니다.
		const bool bExpired = bFlicker
			? Now > Entry.WindowEndSeconds
			: Now >= Entry.WindowEndSeconds;

		if (bExpired)
		{
			Entry.State = EDayOneAnomalyState::Skipped;
			continue;
		}

		const double StartAt = bFlicker
			? PlannedFlickerStartSeconds
			: Entry.WindowStartSeconds;

		if (Now < StartAt || !bCondition)
		{
			continue;
		}

		// 조명은 실제 시작 시각부터 게임 시간 1시간.
		const double EndAt = bFlicker
			? Now + GameTime::SecondsPerHour
			: Entry.WindowEndSeconds;

		StartPresentation(Id, Now, EndAt);
	}
}

void UDayOneAnomalySubsystem::StartPresentation(
	EDayOneAnomaly Presentation,
	double Now,
	double EndAt)
{
	auto& Entry = Progress.FindChecked(Presentation);

	if (Entry.State != EDayOneAnomalyState::Waiting)
	{
		return;
	}

	if (Presentation == EDayOneAnomaly::LobbyFlicker)
	{
		if (!StartLobbyFlicker())
		{
			return;
		}
	}
	else if (Presentation == EDayOneAnomaly::ShadowFigure)
	{
		if (!StartShadowPresentation())
		{
			return;
		}
	}
	else if (Presentation == EDayOneAnomaly::DoorKnock)
	{
		if (!StartDoorKnock())
		{
			return;
		}
	}

	Entry.State = EDayOneAnomalyState::Active;
	Entry.StartedAtSeconds = Now;
	Entry.EndAtSeconds = EndAt;
	
// 조명은 로비에 있을 때 시작하므로 이를 목격으로 인정합니다.
// 노크는 플레이어 근처에서 재생이 시작된 것을 청취로 인정합니다.
// 형체는 CCTV 시야 검사를 통과해야 목격으로 인정합니다.
if (Presentation == EDayOneAnomaly::LobbyFlicker
	|| Presentation == EDayOneAnomaly::DoorKnock)
{
	ReportPresentationWitnessed(Presentation);
}

	OnPresentationStarted.Broadcast(Presentation);
}

void UDayOneAnomalySubsystem::EndPresentation(
	EDayOneAnomaly Presentation)
{
	auto* Entry = Progress.Find(Presentation);

	if (!Entry || Entry->State != EDayOneAnomalyState::Active)
	{
		return;
	}

	Entry->State = EDayOneAnomalyState::Finished;

	if (Presentation == EDayOneAnomaly::LobbyFlicker)
	{
		StopLobbyFlicker();
	}
	else if (Presentation == EDayOneAnomaly::ShadowFigure)
	{
		StopShadowPresentation();
	}
	else if (Presentation == EDayOneAnomaly::DoorKnock)
	{
		StopDoorKnock();
	}

	OnPresentationEnded.Broadcast(Presentation);
}

void UDayOneAnomalySubsystem::StopSchedule()
{
	bScheduleActive = false;
	for (EDayOneAnomaly Id : Presentations)
	{
		EndPresentation(Id);
		if (auto* Entry = Progress.Find(Id); Entry && Entry->State == EDayOneAnomalyState::Waiting)
			Entry->State = EDayOneAnomalyState::Skipped;
	}
}

void UDayOneAnomalySubsystem::SetLobbyLightActors(
    const TArray<AActor*>& Actors)
{
    // 진행 도중 대상이 바뀌면 원상 복구가 꼬일 수 있으므로 유지합니다.
    if (bLobbyFlickering)
    {
        return;
    }

    LobbyLights.Reset();

    for (AActor* Actor : Actors)
    {
        if (!IsValid(Actor) || Actor->GetWorld() != GetWorld())
        {
            continue;
        }

        TArray<ULightComponent*> FoundLights;
        Actor->GetComponents<ULightComponent>(FoundLights);

        for (ULightComponent* Light : FoundLights)
        {
            if (IsValid(Light))
            {
                LobbyLights.AddUnique(
                    TWeakObjectPtr<ULightComponent>(Light));
            }
        }
    }

    if (Clock)
    {
        Evaluate(Clock->GetTotalGameSeconds());
    }
}

void UDayOneAnomalySubsystem::SetPlayerInLobby(bool bInside)
{
    bInLobby = bInside;

    if (Clock)
    {
        Evaluate(Clock->GetTotalGameSeconds());
    }
}

bool UDayOneAnomalySubsystem::StartLobbyFlicker()
{
    UWorld* World = GetWorld();
    if (!World || bLobbyFlickering)
    {
        return false;
    }

    LobbyOriginalVisibility.Reset();

    for (const TWeakObjectPtr<ULightComponent>& WeakLight : LobbyLights)
    {
        if (ULightComponent* Light = WeakLight.Get())
        {
            LobbyOriginalVisibility.Add(
                WeakLight, Light->IsVisible());
        }
    }

    // 대상을 지정하지 않았거나 모두 사라졌다면 시작하지 않습니다.
    if (LobbyOriginalVisibility.IsEmpty())
    {
        return false;
    }

    bLobbyFlickering = true;
    bLobbyLightOutputOn = true;

    // 첫 깜빡임을 즉시 적용하고 다음 깜빡임을 예약합니다.
    ToggleLobbyFlicker();
    return true;
}

void UDayOneAnomalySubsystem::ToggleLobbyFlicker()
{
    UWorld* World = GetWorld();
    if (!World || !bLobbyFlickering)
    {
        return;
    }

    bLobbyLightOutputOn = !bLobbyLightOutputOn;

    for (const auto& Pair : LobbyOriginalVisibility)
    {
        if (ULightComponent* Light = Pair.Key.Get())
        {
            // 원래 꺼져 있던 조명은 임의로 켜지 않습니다.
            Light->SetVisibility(
                Pair.Value && bLobbyLightOutputOn);
        }
    }

    const float MinInterval =
        FMath::Max(0.02f, LobbyMinFlickerInterval);
    const float MaxInterval =
        FMath::Max(MinInterval, LobbyMaxFlickerInterval);

    World->GetTimerManager().SetTimer(
        LobbyFlickerTimer,
        this,
        &UDayOneAnomalySubsystem::ToggleLobbyFlicker,
        FMath::FRandRange(MinInterval, MaxInterval),
        false);
}

void UDayOneAnomalySubsystem::StopLobbyFlicker()
{
    bLobbyFlickering = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LobbyFlickerTimer);
    }

    for (const auto& Pair : LobbyOriginalVisibility)
    {
        if (ULightComponent* Light = Pair.Key.Get())
        {
            Light->SetVisibility(Pair.Value);
        }
    }

    LobbyOriginalVisibility.Reset();
    bLobbyLightOutputOn = true;
}

void UDayOneAnomalySubsystem::SetShadowPresentationTargets(
    ACCTVMonitor* Monitor,
    AShadowFigureActor* LobbyFigure,
    AShadowFigureActor* SecondFloorFigure)
{
    UWorld* World = GetWorld();

    if (!World || !IsValid(Monitor) || Monitor->GetWorld() != World)
    {
        return;
    }

    // 이미 시작한 연출의 대상은 중간에 교체하지 않습니다.
    if (GetProgress(EDayOneAnomaly::ShadowFigure).State
        == EDayOneAnomalyState::Active)
    {
        return;
    }

    ShadowCCTVMonitor = Monitor;
    ShadowCandidates.Reset();

    auto AddCandidate = [this, World](AShadowFigureActor* Figure)
    {
        if (IsValid(Figure) && Figure->GetWorld() == World)
        {
            ShadowCandidates.AddUnique(
                TWeakObjectPtr<AShadowFigureActor>(Figure));
        }
    };

    AddCandidate(LobbyFigure);
    AddCandidate(SecondFloorFigure);

    // 기존 CCTV 클래스에 새 이벤트를 추가하지 않고 상태를 읽습니다.
    World->GetTimerManager().SetTimer(
        CCTVViewingTimer,
        this,
        &UDayOneAnomalySubsystem::PollCCTVViewing,
        0.1f,
        true);

    PollCCTVViewing();
}

void UDayOneAnomalySubsystem::PollCCTVViewing()
{
    if (!bScheduleActive)
    {
        return;
    }

    const ACCTVMonitor* Monitor = ShadowCCTVMonitor.Get();

    SetCCTVViewing(
        Monitor && Monitor->IsViewingCCTV());
}

bool UDayOneAnomalySubsystem::StartShadowPresentation()
{
    TArray<AShadowFigureActor*> Available;

    for (const TWeakObjectPtr<AShadowFigureActor>& WeakFigure
        : ShadowCandidates)
    {
        AShadowFigureActor* Figure = WeakFigure.Get();

        if (!Figure || !Figure->HasActorBegunPlay())
        {
            continue;
        }

        // 이미 등장 중인 형체는 가져오지 않습니다.
        if (Figure->IsPresent())
        {
            continue;
        }

        Available.Add(Figure);
    }

    if (Available.IsEmpty())
    {
        // 대상이 준비되지 않았다면 Waiting 상태를 유지합니다.
        return false;
    }

    // 이번 시간대에는 한 위치만 선택합니다.
    // CCTV를 껐다 켜도 다시 추첨하지 않습니다.
    ActiveShadowFigure =
        Available[FMath::RandRange(0, Available.Num() - 1)];

    UpdateShadowVisibility();
    return true;
}

void UDayOneAnomalySubsystem::UpdateShadowVisibility()
{
    AShadowFigureActor* Figure = ActiveShadowFigure.Get();

    if (!Figure)
    {
        return;
    }

    if (bViewingCCTV)
    {
        if (!Figure->IsPresent())
        {
            // 기존 메시·머티리얼·CCTV 전용 표시 로직을 재사용합니다.
            Figure->Appear();

            // 첫날은 관찰용이므로 일반 조준/상호작용 충돌을 끕니다.
            Figure->SetActorEnableCollision(false);
        }
    }
    else if (Figure->IsPresent())
    {
        // CCTV를 끄면 실제 위치에서도 형체가 남지 않도록 숨깁니다.
        Figure->Disappear();
    }
}

void UDayOneAnomalySubsystem::StopShadowPresentation()
{
    if (AShadowFigureActor* Figure = ActiveShadowFigure.Get())
    {
        Figure->Disappear();
    }

    ActiveShadowFigure.Reset();
	ShadowVisibleSinceSeconds = -1.0;
}

void UDayOneAnomalySubsystem::ConfigureDoorKnock(
    TSubclassOf<AKnockDoorActor> InKnockClass,
    ACCTVMonitor* Monitor)
{
    if (!InKnockClass || ActiveKnockActor.IsValid())
    {
        return;
    }

    DoorKnockClass = InKnockClass;

    // 형체와 같은 CCTV 모니터를 사용합니다.
    // 기존 변수 이름은 유지합니다.
    if (IsValid(Monitor) && Monitor->GetWorld() == GetWorld())
    {
        ShadowCCTVMonitor = Monitor;

        GetWorld()->GetTimerManager().SetTimer(
            CCTVViewingTimer,
            this,
            &UDayOneAnomalySubsystem::PollCCTVViewing,
            0.1f,
            true);

        PollCCTVViewing();
    }

    if (Clock)
    {
        Evaluate(Clock->GetTotalGameSeconds());
    }
}

bool UDayOneAnomalySubsystem::StartDoorKnock()
{
    UWorld* World = GetWorld();
    APlayerController* PC =
        World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;

    if (!World || !Pawn || !DoorKnockClass)
    {
        return false;
    }

    // 플레이어 주변 1.5~2.5m 중 한 지점을 선택합니다.
    const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
    const float Distance = FMath::FRandRange(150.0f, 250.0f);

    DoorKnockWorldLocation =
        Pawn->GetActorLocation()
        + FVector(
            FMath::Cos(Angle) * Distance,
            FMath::Sin(Angle) * Distance,
            80.0f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transient;

    AKnockDoorActor* Actor = World->SpawnActor<AKnockDoorActor>(
        DoorKnockClass.Get(),
        DoorKnockWorldLocation,
        FRotator::ZeroRotator,
        SpawnParams);

    if (!IsValid(Actor))
    {
        return false;
    }

    // 기존 클래스가 생성하는 KnockAudio 컴포넌트를 사용합니다.
    UAudioComponent* Audio = nullptr;
    TArray<UAudioComponent*> AudioComponents;
    Actor->GetComponents<UAudioComponent>(AudioComponents);

    for (UAudioComponent* Component : AudioComponents)
    {
        if (IsValid(Component)
            && Component->GetFName() == FName(TEXT("KnockAudio")))
        {
            Audio = Component;
            break;
        }
    }

    if (!Audio)
    {
        Actor->Destroy();
        return false;
    }

    ActiveKnockActor = Actor;
    ActiveKnockAudio = Audio;

    // 소리만 사용하며 해결용 말발굽 기능은 켜지 않습니다.
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->SetAwaitingHorseshoe(false);
    Actor->StopKnock();

    // 원본 사운드 에셋을 수정하지 않고,
    // 이번에 생성한 오디오 컴포넌트에만 공간감을 설정합니다.
    FSoundAttenuationSettings Attenuation;
    Attenuation.bAttenuate = true;
    Attenuation.bSpatialize = true;
    Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation.AttenuationShapeExtents = FVector(300.0f);
    Attenuation.FalloffDistance = 1200.0f;

    Audio->bAllowSpatialization = true;
    Audio->AdjustAttenuation(Attenuation);

    NextDoorKnockSeconds = World->GetTimeSeconds();
    TickDoorKnock();

    // BP 기본값에 KnockSound가 지정되지 않았다면 시작 실패.
    if (!Audio->GetSound())
    {
        StopDoorKnock();
        return false;
    }

    World->GetTimerManager().SetTimer(
        DoorKnockTickTimer,
        this,
        &UDayOneAnomalySubsystem::TickDoorKnock,
        0.1f,
        true);

    World->GetTimerManager().SetTimer(
        DoorKnockEndTimer,
        this,
        &UDayOneAnomalySubsystem::FinishDoorKnock,
        DoorKnockDurationSeconds,
        false);

    return true;
}

void UDayOneAnomalySubsystem::TickDoorKnock()
{
    UWorld* World = GetWorld();
    AKnockDoorActor* Actor = ActiveKnockActor.Get();
    UAudioComponent* Audio = ActiveKnockAudio.Get();
    APlayerController* PC =
        World ? World->GetFirstPlayerController() : nullptr;
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;

    if (!World || !Actor || !Audio || !Pawn)
    {
        FinishDoorKnock();
        return;
    }

    FVector PlaybackLocation = DoorKnockWorldLocation;

    const ACCTVMonitor* Monitor = ShadowCCTVMonitor.Get();
    const bool bWatchingCCTV =
        Monitor && Monitor->IsViewingCCTV();

    if (bWatchingCCTV)
    {
        // CCTV는 청취 위치도 원격 카메라로 이동할 수 있습니다.
        // 플레이어 주변에서 정한 소리의 상대 위치를
        // 현재 청취 위치에 옮겨 이 노크만 가까이 들리게 합니다.
        FVector ListenerLocation;
        FVector ListenerForward;
        FVector ListenerRight;

        PC->GetAudioListenerPosition(
            ListenerLocation,
            ListenerForward,
            ListenerRight);

        const FVector LocalOffset =
            Pawn->GetActorQuat().UnrotateVector(
                DoorKnockWorldLocation - Pawn->GetActorLocation());

        const FQuat ListenerRotation =
            FRotationMatrix::MakeFromXY(
                ListenerForward, ListenerRight).ToQuat();

        PlaybackLocation =
            ListenerLocation
            + ListenerRotation.RotateVector(LocalOffset);
    }

    Actor->SetActorLocation(PlaybackLocation);

    const double Now = World->GetTimeSeconds();

    // 재생 중인 사운드를 중간에 재시작하지 않습니다.
    if (Now >= NextDoorKnockSeconds && !Audio->IsPlaying())
    {
        Actor->PlayKnock();
        NextDoorKnockSeconds = Now + DoorKnockRepeatInterval;
    }
}

void UDayOneAnomalySubsystem::FinishDoorKnock()
{
    EndPresentation(EDayOneAnomaly::DoorKnock);

    // 시작 실패 등 Active 상태 이전의 정리도 보장합니다.
    StopDoorKnock();
}

void UDayOneAnomalySubsystem::StopDoorKnock()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DoorKnockTickTimer);
        World->GetTimerManager().ClearTimer(DoorKnockEndTimer);
    }

    if (AKnockDoorActor* Actor = ActiveKnockActor.Get())
    {
        Actor->StopKnock();
        Actor->Destroy();
    }

    ActiveKnockActor.Reset();
    ActiveKnockAudio.Reset();
    NextDoorKnockSeconds = 0.0;
}

void UDayOneAnomalySubsystem::ReportPresentationWitnessed(
    EDayOneAnomaly Presentation)
{
    if (WitnessedPresentations.Contains(Presentation))
    {
        return;
    }

    FName EventId;

    switch (Presentation)
    {
    case EDayOneAnomaly::LobbyFlicker:
        EventId = TEXT("DayOne.LightWitnessed");
        break;

    case EDayOneAnomaly::ShadowFigure:
        EventId = TEXT("DayOne.ShadowWitnessed");
        break;

    case EDayOneAnomaly::DoorKnock:
        EventId = TEXT("DayOne.KnockHeard");
        break;

    default:
        return;
    }

    UMemoSubsystem* Memos =
        GetWorld()->GetSubsystem<UMemoSubsystem>();

    if (!Memos)
    {
        return;
    }

    WitnessedPresentations.Add(Presentation);
    Memos->ReportMemoEvent(EventId);
}

void UDayOneAnomalySubsystem::CheckShadowWitness()
{
    const EDayOneAnomaly Id = EDayOneAnomaly::ShadowFigure;

    if (WitnessedPresentations.Contains(Id))
    {
        return;
    }

    UWorld* World = GetWorld();
    AShadowFigureActor* Figure = ActiveShadowFigure.Get();
    APlayerController* PC =
        World ? World->GetFirstPlayerController() : nullptr;

    if (!bViewingCCTV
        || GetProgress(Id).State != EDayOneAnomalyState::Active
        || !Figure
        || !Figure->IsPresent()
        || !PC
        || !PC->PlayerCameraManager)
    {
        ShadowVisibleSinceSeconds = -1.0;
        return;
    }

    FVector Center;
    FVector Extent;
    Figure->GetActorBounds(false, Center, Extent);

    FVector2D ScreenPosition;
    int32 Width = 0;
    int32 Height = 0;
    PC->GetViewportSize(Width, Height);

    const bool bOnScreen =
        Width > 0 && Height > 0
        && PC->ProjectWorldLocationToScreen(Center, ScreenPosition)
        && ScreenPosition.X >= 0.0
        && ScreenPosition.X <= Width
        && ScreenPosition.Y >= 0.0
        && ScreenPosition.Y <= Height;

    if (!bOnScreen)
    {
        ShadowVisibleSinceSeconds = -1.0;
        return;
    }

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Figure);

    if (APawn* Pawn = PC->GetPawn())
    {
        Params.AddIgnoredActor(Pawn);
    }

    FHitResult Hit;

    const bool bBlocked = World->LineTraceSingleByChannel(
        Hit,
        PC->PlayerCameraManager->GetCameraLocation(),
        Center,
        ECC_Visibility,
        Params);

    if (bBlocked)
    {
        ShadowVisibleSinceSeconds = -1.0;
        return;
    }

    const double Now = World->GetTimeSeconds();

    if (ShadowVisibleSinceSeconds < 0.0)
    {
        ShadowVisibleSinceSeconds = Now;
    }

    // CCTV 화면 안에서 가려지지 않은 상태를 0.5초 유지하면 목격.
    if (Now - ShadowVisibleSinceSeconds >= 0.5)
    {
        ReportPresentationWitnessed(Id);
    }
}
