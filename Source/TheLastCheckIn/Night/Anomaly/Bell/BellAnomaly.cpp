#include "Night/Anomaly/Bell/BellAnomaly.h"
#include "Night/Anomaly/Bell/BellActor.h"
#include "Rooms/RoomActor.h"
#include "Rooms/RoomQuery.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABellAnomaly::ABellAnomaly()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AnomalyId = FName("Bell");
	DisplayName = NSLOCTEXT("Anomaly", "BellName", "울리는 종");
	ComplaintMessage = NSLOCTEXT("Anomaly", "BellComplaint", "종 소리가 시끄러워서 편히 쉴 수가 없어요. 저 종소리 좀 멈춰주세요!");

	/*
	18시 ~ 다음 날 5시 사이에 발생 - 5시엔 정산이 시작되며 매니저가 Fail 처리하므로 시작은 4시까지
	매니저가 시작 후 EscalateAfterMinutes가 지나면 Escalate 호출, 같은 층 투숙객이 없으면 다음 시간 갱신 때 다시 시도
	*/
	EarliestStartHour = 18;
	LatestStartHour = 4;
	EscalateAfterMinutes = 60;
}

void ABellAnomaly::BeginPlay()
{
	Super::BeginPlay();

	if (Bells.IsEmpty())
	{
		for (TActorIterator<ABellActor> It(GetWorld()); It; ++It)
		{
			Bells.Add(*It);
		}
	}
	for (ABellActor* Bell : Bells)
	{
		if (Bell)
		{
			Bell->OnBroken.AddUniqueDynamic(this, &ThisClass::HandleBellBroken);
		}
	}
}

bool ABellAnomaly::StartAnomaly()
{
	TArray<ABellActor*> Candidates;
	for (ABellActor* Bell : Bells)
	{
		if (CanRing(Bell))
		{
			Candidates.Add(Bell);
		}
	}
	if (Candidates.IsEmpty() || !Super::StartAnomaly())
	{
		return false;
	}

	RingingBell = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	RingingBell->StartRinging();
	return true;
}

/*
방치 - 울리는 종과 같은 층의 투숙 객실에서 컴플레인 전화
*/
bool ABellAnomaly::Escalate()
{
	if (!RingingBell)
	{
		return false;
	}

	const int32 Floor = RingingBell->GetFloor();
	TArray<ARoomActor*> Rooms;
	RoomQuery::GetRooms(GetWorld(), Rooms);
	TArray<ARoomActor*> Occupied;
	for (ARoomActor* Room : Rooms)
	{
		if (Room->GetRoomState() == ERoomState::Occupied && Room->GetRoomNumber() / 100 == Floor)
		{
			Occupied.Add(Room);
		}
	}
	if (Occupied.IsEmpty() || !Super::Escalate())
	{
		return false;
	}

	const ARoomActor* Caller = Occupied[FMath::RandRange(0, Occupied.Num() - 1)];
	const FText RoomText = FText::AsNumber(Caller->GetRoomNumber(), &FNumberFormattingOptions::DefaultNoGrouping());
	RaiseComplaint(FText::FormatNamed(ComplaintMessage, TEXT("Room"), RoomText));
	return true;
}

/*
밤이 끝날 때까지 못 멈춤 - 컴플레인까지 갔으면 평판 감소
*/
bool ABellAnomaly::Fail()
{
	if (!Super::Fail())
	{
		return false;
	}
	if (RingingBell)
	{
		RingingBell->StopRinging();
		RingingBell = nullptr;
	}
	return true;
}

/*
울리던 종을 부숨 - 해결 (해결 알림은 매니저가 띄움)
*/
void ABellAnomaly::HandleBellBroken(ABellActor* Bell)
{
	if (Bell && Bell == RingingBell)
	{
		RingingBell = nullptr;
		Resolve();
	}
}

/*
걸려 있는 종 중 연결된 객실이 비어 있거나 아직 해금되지 않은 종만
*/
bool ABellAnomaly::CanRing(const ABellActor* Bell) const
{
	if (!Bell || Bell->GetBellState() != EBellState::Idle)
	{
		return false;
	}
	const ARoomActor* Room = FindRoom(Bell->GetRoomNumber());
	return Room && (!Room->IsUnlocked() || Room->GetRoomState() == ERoomState::Available);
}

ARoomActor* ABellAnomaly::FindRoom(int32 RoomNumber) const
{
	return RoomQuery::FindRoom(GetWorld(), RoomNumber);
}
