// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RoomDataTypes.h"
#include "RoomActor.generated.h"

class USceneComponent;

UCLASS()
class THELASTCHECKIN_API ARoomActor : public AActor
{
	GENERATED_BODY()

public:
	ARoomActor();
	
	UFUNCTION(BlueprintPure, Category = "Room")
	int32 GetRoomNumber() const
	{
		return RoomNumber;
	}

	UFUNCTION(BlueprintPure, Category = "Room")
	ERoomState GetRoomState() const
	{
		return RoomState;
	}

	UFUNCTION(BlueprintPure, Category = "Room")
	AActor* GetArrivalTarget() const
	{
		return ArrivalTarget.Get();
	}

	UFUNCTION(BlueprintPure, Category = "Room")
	int32 GetCapacity() const;

	/*
	UFUNCTION(BlueprintPure, Category = "Room")
	float GetBaseRoomRate() const;
	*/
	
	// 객실 배정
	bool TryReserve(const FGuid& InStayId);
	
	// 배정 취소
	bool CancelReservation(const FGuid& InStayId);
	
	// 배정된 숙박 입실 완료 처리
	bool MarkOccupied(const FGuid& InStayId);
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<USceneComponent> SceneRoot;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room")
	int32 RoomNumber;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FDataTableRowHandle RoomTypeRow;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room")
	TObjectPtr<AActor> ArrivalTarget;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room")
	ERoomState RoomState = ERoomState::Available;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Room")
	FGuid CurrentStayId;
};
