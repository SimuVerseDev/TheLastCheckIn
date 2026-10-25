#pragma once
#include "CoreMinimal.h"
#include "Housekeeping/Core/Dirt/HKDirtComponent.h"
#include "Components/BoxComponent.h"
#include "HKTrashDirtComponent.generated.h"
class UBoxComponent;

UCLASS(ClassGroup=(HOUSEKEEPING), meta=(BlueprintSpawnableComponent))
class UHKTrashDirtComponent : public UHKDirtComponent
{
	GENERATED_BODY()
	
public:
	UHKTrashDirtComponent();
	
	UBoxComponent* GetRoomBounds() const
	{
		return RoomBounds.Get();
	}
	
	void SetRoomBounds(UBoxComponent* InRoomBounds)
	{
		RoomBounds = InRoomBounds;
	}
protected:
	virtual float EvaluateDirt() const override;
	
private:
	TWeakObjectPtr<UBoxComponent> RoomBounds;
};