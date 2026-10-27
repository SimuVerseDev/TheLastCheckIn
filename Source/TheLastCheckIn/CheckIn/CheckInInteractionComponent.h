// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CheckInInteractionComponent.generated.h"

class AGuestCharacter;
class ARoomActor;
class APlayerController;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCheckInRoomListChanged);

UCLASS(ClassGroup=(CheckIn), meta=(BlueprintSpawnableComponent))
class THELASTCHECKIN_API UCheckInInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCheckInInteractionComponent();
	
	UFUNCTION(BlueprintCallable, Category ="CheckIn")
	void BeginCounterInteraction(AActor* InCounterTarget);
	
	UFUNCTION(BlueprintCallable, Category ="CheckIn")
	void EndCounterInteraction();
	
	UFUNCTION(BlueprintPure, Category="CheckIn")
	bool CanOpenCheckIn() const;
	
	UFUNCTION(BlueprintCallable, Category="CheckIn")
	bool OpenCheckIn();

	UFUNCTION(BlueprintPure, Category="CheckIn")
	bool IsCheckInOpen() const
	{
		return bCheckInOpen;
	}
	
	UFUNCTION(BlueprintCallable, Category="CheckIn")
	void CloseCheckIn();

	UFUNCTION(BlueprintCallable, Category="CheckIn")
	bool TryAssignRoomNumber(int32 RoomNumber);

	UFUNCTION(BlueprintPure, Category="CheckIn")
	void GetRoomStatus(
		int32 RoomNumber,
		bool& bRoomExists,
		bool& bAssigned,
		bool& bSelectable) const;

	UPROPERTY(BlueprintAssignable, Category="CheckIn")
	FCheckInRoomListChanged OnRoomListChanged;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditAnywhere, Category="CheckIn|UI")
	TSubclassOf<UUserWidget> CheckInWidgetClass;
	
	UPROPERTY(EditAnywhere, Category="CheckIn|Detection", 
		meta=(ClampMin="0.05", Units="s"))
	float CheckInterval =0.2f;
	
	//손님이 카운터로부터 대기해야되는 거리
	UPROPERTY(EditAnywhere, Category="CheckIn|Detection", 
		meta=(ClampMin="1.0", Units="cm"))
	float GuestRange =200.0f;
	
private:
	void CheckInitialCounterOverlap();
	void CheckCounterState();

	bool IsCurrentGuestReady() const;
	bool IsCarryingObject() const;

	AGuestCharacter* FindWaitingGuest() const;
	ARoomActor* FindUniqueRoom(int32 RoomNumber) const;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CheckInWidget;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> LocalController;

	TWeakObjectPtr<AActor> CounterTarget;
	TWeakObjectPtr<AGuestCharacter> CurrentGuest;

	FTimerHandle InitialCounterCheckTimer;
	FTimerHandle CounterCheckTimer;

	bool bInsideCounter = false;
	bool bCheckInOpen = false;
	bool bAssignedThisSession = false;
	bool bPreviousMouseCursor = false;
	
};
