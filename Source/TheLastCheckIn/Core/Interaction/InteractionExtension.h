#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionExtension.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UInteractionExtension : public UInterface
{
	GENERATED_BODY()
};

/*
플레이어의 기능 컴포넌트(잡기·카트 등)가 공통 상호작용 컴포넌트에 끼어드는 지점
*/
class IInteractionExtension
{
	GENERATED_BODY()

public:
	virtual AActor* FindInteractionTarget(const FVector& Start, const FVector& End) const
	{
		return nullptr;
	}
	
	virtual bool BlocksFocus() const
	{
		return false;
	}
	
	virtual FText GetBlockingPrompt() const
	{
		return FText::GetEmpty();
	}
	
	virtual void OnUseReleased()
	{
	}
};