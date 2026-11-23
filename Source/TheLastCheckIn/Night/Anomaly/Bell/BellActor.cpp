#include "Night/Anomaly/Bell/BellActor.h"
#include "Night/Anomaly/Bell/BellRopeItem.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

namespace BellActorDefaults
{
	/*
	antique_brass_bell 메시는 세 개 모두 걸이 바닥이 원점 - 종 몸체 윗부분(걸이에 매달린 곳)을 흔들림 축으로
	*/
	const FVector PivotLocation(-19.6f, 0.0f, 45.0f);

	// 종 몸체(bell03) 바운드 중심
	const FVector BodyCenter(-19.6f, 0.0f, 27.8f);
}

ABellActor::ABellActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Hanger = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hanger"));
	Hanger->SetupAttachment(Root);

	SwingPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwingPivot"));
	SwingPivot->SetupAttachment(Root);
	SwingPivot->SetMobility(EComponentMobility::Movable);
	SwingPivot->SetRelativeLocation(BellActorDefaults::PivotLocation);

	// 몸체와 추는 피벗 아래에 붙이되 원래 자리(액터 원점 기준)에 보이도록 피벗만큼 되돌려 둠
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SwingPivot);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetRelativeLocation(-BellActorDefaults::PivotLocation);
	// 상호작용 시선 트레이스(Visibility)에 잡히도록
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Clapper = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Clapper"));
	Clapper->SetupAttachment(SwingPivot);
	Clapper->SetMobility(EComponentMobility::Movable);
	Clapper->SetRelativeLocation(-BellActorDefaults::PivotLocation);

	// 몸체와 같은 기준으로 배치 - 피벗 아래라 좌우 반전도 함께 적용됨
	TiedRope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TiedRope"));
	TiedRope->SetupAttachment(SwingPivot);
	TiedRope->SetMobility(EComponentMobility::Movable);
	TiedRope->SetRelativeLocation(-BellActorDefaults::PivotLocation);
	TiedRope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TiedRope->SetHiddenInGame(true);

	/*
	상호작용 트레이스(Visibility)만 막고 이동이나 물리에는 관여하지 않음
	피벗 아래에 있어 같이 흔들리고, 종을 치우면 다른 부품과 함께 꺼짐
	*/
	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(SwingPivot);
	InteractBox->SetRelativeLocation(BellActorDefaults::BodyCenter - BellActorDefaults::PivotLocation);
	InteractBox->SetBoxExtent(FVector(18.0f, 18.0f, 18.0f));
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	RingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RingAudio"));
	RingAudio->SetupAttachment(Root);
	RingAudio->bAutoActivate = false;

	NameTag = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(Root);

	BreakPrompt = NSLOCTEXT("Anomaly", "BellBreak", "종 부수기");
	RemovePrompt = NSLOCTEXT("Anomaly", "BellRemove", "부서진 종 치우기");
	TiePrompt = NSLOCTEXT("Anomaly", "BellTie", "줄로 묶기");
}

/*
에디터에서 RoomNumber, bMirrored를 바꾸면 바로 반영
*/
void ABellActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncFromChildActorTemplate();
	ApplyMirror();
	UpdateNameTag();
}

/*
종 판(BP_RoomBellSet)의 Child Actor로 생성됐을 때, 칸별 RoomNumber와 bMirrored를 BP 쪽 원본 템플릿에서 가져옴
게임 시작 시 Child Actor가 템플릿 없이 기본값으로 다시 생성되는 경우가 있어서 필요
종 판 밖에 단독으로 놓인 종은 자기 값을 그대로 씀
*/
void ABellActor::SyncFromChildActorTemplate()
{
	const UChildActorComponent* ParentChildActor = Cast<UChildActorComponent>(GetParentComponent());
	if (!ParentChildActor)
	{
		return;
	}
	const ABellActor* Template = Cast<ABellActor>(ParentChildActor->GetChildActorTemplate());
	if (!Template || Template == this)
	{
		if (const UChildActorComponent* Archetype = Cast<UChildActorComponent>(ParentChildActor->GetArchetype()))
		{
			Template = Cast<ABellActor>(Archetype->GetChildActorTemplate());
		}
	}
	if (!Template || Template == this)
	{
		return;
	}
	RoomNumber = Template->RoomNumber;
	bMirrored = Template->bMirrored;
}

/*
기본값(에디터, 컴포넌트 재등록 시 이 값으로 초기화됨)과 런타임 값을 모두 갱신
*/
void ABellActor::UpdateNameTag()
{
	const float Value = static_cast<float>(RoomNumber);
	NameTag->SetDefaultCustomPrimitiveDataFloat(0, Value);
	NameTag->SetCustomPrimitiveDataFloat(0, Value);
}

/*
클래스 기본값(BP 설정 포함)을 기준으로 다시 계산 - 여러 번 호출돼도 누적되지 않음
걸이와 피벗은 로컬 X로 뒤집고, 이름표와 소리는 위치만 옮김
*/
void ABellActor::ApplyMirror()
{
	const ABellActor* Defaults = GetClass()->GetDefaultObject<ABellActor>();
	if (!Defaults || Defaults == this)
	{
		return;
	}
	const float Sign = bMirrored ? -1.0f : 1.0f;
	const auto MirrorFrom = [Sign](USceneComponent* Target, const USceneComponent* Source, bool bFlipScale)
	{
		if (!Target || !Source)
		{
			return;
		}
		FVector Location = Source->GetRelativeLocation();
		Location.X *= Sign;
		Target->SetRelativeLocation(Location);
		if (bFlipScale)
		{
			FVector Scale = Source->GetRelativeScale3D();
			Scale.X *= Sign;
			Target->SetRelativeScale3D(Scale);
		}
	};
	MirrorFrom(Hanger, Defaults->Hanger, true);
	MirrorFrom(SwingPivot, Defaults->SwingPivot, true);
	MirrorFrom(NameTag, Defaults->NameTag, false);
	MirrorFrom(RingAudio, Defaults->RingAudio, false);
}

void ABellActor::BeginPlay()
{
	Super::BeginPlay();
	// 에디터에서 Child Actor로 저장된 값이 예전 상태로 남아 있어도 게임 시작 시 다시 맞춤
	SyncFromChildActorTemplate();
	ApplyMirror();
	UpdateNameTag();
	RingAudio->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleRingSoundFinished);
	OriginalMesh = Mesh->GetStaticMesh();
	PivotHomeRotation = SwingPivot->GetRelativeRotation();

	// 치울 때 끄고 복구할 때 되돌릴 원래 콜리전 - BP에서 피벗 아래에 추가한 부품 포함
	TArray<USceneComponent*> Parts;
	SwingPivot->GetChildrenComponents(true, Parts);
	for (USceneComponent* Part : Parts)
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Part))
		{
			SwingPartCollisions.Add(Prim, Prim->GetCollisionEnabled());
		}
	}
}

void ABellActor::StartRinging()
{
	if (BellState != EBellState::Idle)
	{
		return;
	}
	BellState = EBellState::Ringing;
	RingElapsed = 0.0f;
	if (RingSound)
	{
		RingAudio->SetSound(RingSound);
	}
	if (RingInterval > 0.0f)
	{
		PlayOverlapRing();
		GetWorldTimerManager().SetTimer(RingRepeatTimer, this, &ThisClass::PlayOverlapRing, RingInterval, true);
	}
	else
	{
		PlayRingSound();
	}
	SetActorTickEnabled(true);
	OnRingStarted();
}

void ABellActor::PlayRingSound()
{
	if (BellState == EBellState::Ringing && RingAudio->Sound)
	{
		RingAudio->SetPitchMultiplier(RingPlaybackSpeed);
		RingAudio->Play();
	}
}

/*
겹침 모드 - RingAudio 설정(위치, 볼륨, 감쇠)을 복사한 새 소리를 하나 더 재생, 끝나면 자동 파괴
*/
void ABellActor::PlayOverlapRing()
{
	if (BellState != EBellState::Ringing || !RingAudio->Sound)
	{
		return;
	}
	OverlapVoices.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Voice)
	{
		return !Voice.IsValid() || !Voice->IsPlaying();
	});

	UAudioComponent* Voice = NewObject<UAudioComponent>(this);
	Voice->SetSound(RingAudio->Sound);
	Voice->VolumeMultiplier = RingAudio->VolumeMultiplier;
	Voice->PitchMultiplier = RingPlaybackSpeed;
	Voice->bOverrideAttenuation = RingAudio->bOverrideAttenuation;
	Voice->AttenuationSettings = RingAudio->AttenuationSettings;
	Voice->AttenuationOverrides = RingAudio->AttenuationOverrides;
	Voice->bAutoActivate = false;
	Voice->bAutoDestroy = true;
	Voice->SetupAttachment(RingAudio);
	Voice->RegisterComponent();
	Voice->Play();
	OverlapVoices.Add(Voice);
}

/*
반복 타이머와 재생 중인 소리 모두 정지
*/
void ABellActor::StopRingSound()
{
	GetWorldTimerManager().ClearTimer(RingRepeatTimer);
	RingAudio->Stop();
	for (const TWeakObjectPtr<UAudioComponent>& Voice : OverlapVoices)
	{
		if (UAudioComponent* Comp = Voice.Get())
		{
			if (RingStopFadeTime > 0.0f)
			{
				Comp->FadeOut(RingStopFadeTime, 0.0f);
			}
			else
			{
				Comp->Stop();
			}
		}
	}
	OverlapVoices.Reset();
}

/*
단발 사운드라도 울리는 동안은 계속 반복 - 부수거나 멈추면 상태가 먼저 바뀌어 여기서 끊김
*/
void ABellActor::HandleRingSoundFinished()
{
	if (BellState != EBellState::Ringing)
	{
		return;
	}
	if (RingRepeatDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RingRepeatTimer, this, &ThisClass::PlayRingSound, RingRepeatDelay, false);
	}
	else
	{
		PlayRingSound();
	}
}

void ABellActor::StopRinging()
{
	if (BellState != EBellState::Ringing)
	{
		return;
	}
	BellState = EBellState::Idle;
	StopRingSound();
	SetActorTickEnabled(false);
	SwingPivot->SetRelativeRotation(PivotHomeRotation);
}

void ABellActor::RestoreBell()
{
	if (BellState != EBellState::Removed && BellState != EBellState::Broken && BellState != EBellState::Tied)
	{
		return;
	}
	BellState = EBellState::Idle;
	if (OriginalMesh)
	{
		Mesh->SetStaticMesh(OriginalMesh);
	}
	SwingPivot->SetRelativeRotation(PivotHomeRotation);
	SetSwingPartsPresent(true);
	// 피벗 아래를 다시 보이게 하면서 같이 드러난 줄은 숨김
	TiedRope->SetHiddenInGame(true);
	OnBellRestored();
}

bool ABellActor::CanInteract_Implementation(AActor* Interactor) const
{
	return BellState == EBellState::Ringing || BellState == EBellState::Broken;
}

FText ABellActor::GetInteractPrompt_Implementation(AActor* Interactor) const
{
	return BellState == EBellState::Broken ? RemovePrompt : BreakPrompt;
}

void ABellActor::Interact_Implementation(AActor* Interactor)
{
	if (BellState == EBellState::Ringing)
	{
		Break();
	}
	else if (BellState == EBellState::Broken)
	{
		Remove();
	}
}

/*
울리는 종을 부숨 - 소리 멈추고 부서진 메시로 교체, 이상현상 해결 신호
*/
/*
줄을 든 채 울리는 종을 조준하면 LMB로 묶기
*/
bool ABellActor::CanUse_Implementation(AActor* Interactor) const
{
	return BellState == EBellState::Ringing && ABellRopeItem::FindHeldBy(Interactor) != nullptr;
}

FText ABellActor::GetUsePrompt_Implementation(AActor* Interactor) const
{
	return TiePrompt;
}

bool ABellActor::Use_Implementation(AActor* Interactor)
{
	if (!CanUse_Implementation(Interactor))
	{
		return false;
	}
	Tie();
	return true;
}

/*
울리는 종을 줄로 묶음 - 소리와 흔들림을 멈추고 줄을 보이게 함, 이상현상 해결 신호
묶인 종은 계속 그대로 - 다시 울리지 않고 부수거나 치울 수도 없음
*/
void ABellActor::Tie()
{
	BellState = EBellState::Tied;
	StopRingSound();
	SetActorTickEnabled(false);
	SwingPivot->SetRelativeRotation(PivotHomeRotation);
	TiedRope->SetHiddenInGame(false);
	OnBellTied();
	OnTied.Broadcast(this);
}

void ABellActor::Break()
{
	BellState = EBellState::Broken;
	StopRingSound();
	SetActorTickEnabled(false);
	SwingPivot->SetRelativeRotation(PivotHomeRotation);
	if (BrokenMesh)
	{
		Mesh->SetStaticMesh(BrokenMesh);
	}
	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, GetActorLocation());
	}
	OnBellBroken();
	OnBroken.Broadcast(this);
}

/*
부서진 종을 치움 - 피벗 아래(몸체, 추)만 숨기고 걸이, 이름표 등 나머지는 남김
빈 자리로 남고 RestoreBell 전까지 다시 울리지 않음
*/
void ABellActor::Remove()
{
	BellState = EBellState::Removed;
	SetSwingPartsPresent(false);
	OnBellRemoved();
	OnRemoved.Broadcast(this);
}

void ABellActor::SetSwingPartsPresent(bool bPresent)
{
	SwingPivot->SetHiddenInGame(!bPresent, true);
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionEnabled::Type>>& Pair : SwingPartCollisions)
	{
		if (UPrimitiveComponent* Prim = Pair.Key.Get())
		{
			Prim->SetCollisionEnabled(bPresent ? Pair.Value.GetValue() : ECollisionEnabled::NoCollision);
		}
	}
}

/*
울리는 동안 피벗(매달린 지점)을 축으로 몸체와 추가 함께 좌우로 흔들림
*/
void ABellActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BellState != EBellState::Ringing)
	{
		SetActorTickEnabled(false);
		return;
	}
	RingElapsed += DeltaSeconds;
	const float Swing = SwingAngle * FMath::Sin(RingElapsed * SwingFrequency * UE_TWO_PI);
	SwingPivot->SetRelativeRotation(PivotHomeRotation + FRotator(0.0f, 0.0f, Swing));
}
