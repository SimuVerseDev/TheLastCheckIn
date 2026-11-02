#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_GuestFreeSpace.generated.h"

UCLASS()
class THELASTCHECKIN_API UEnvQueryTest_GuestFreeSpace
	: public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEnvQueryTest_GuestFreeSpace(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
};