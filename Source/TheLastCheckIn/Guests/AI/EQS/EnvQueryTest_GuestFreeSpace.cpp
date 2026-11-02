#include "Guests/AI/EQS/EnvQueryTest_GuestFreeSpace.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Guests/GuestCharacter.h"

UEnvQueryTest_GuestFreeSpace::UEnvQueryTest_GuestFreeSpace(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_Point::StaticClass();

	SetWorkOnFloatValues(false);

	TestPurpose = EEnvTestPurpose::Filter;
	FilterType = EEnvTestFilterType::Match;
	BoolValue.DefaultValue = true;
}

void UEnvQueryTest_GuestFreeSpace::RunTest(FEnvQueryInstance& QueryInstance) const
{
	// BT의 Run EQS Query는 조종 중인 Pawn을 Querier로 사용
	const AGuestCharacter* Guest =
		Cast<AGuestCharacter>(QueryInstance.Owner.Get());

	BoolValue.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);

	const bool bExpected = BoolValue.GetValue();

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance);
		 It; ++It)
	{
		const FVector Location =
			GetItemLocation(QueryInstance, It.GetIndex());

		const bool bUsable =
			IsValid(Guest) &&
			Guest->CanReserveMoveLocation(Location);

		It.SetScore(
			TestPurpose, FilterType, bUsable, bExpected);
	}
}