#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Memo/MemoType.h"
#include "MemoSubsystem.generated.h"

class UDataTable;
class UHotelPhaseSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FMemoStateChanged,
	FName, MemoId,
	EMemoState, NewState);

UCLASS(BlueprintType)
class THELASTCHECKIN_API UMemoSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(
		FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	// 모든 메모지는 같은 데이터 테이블을 사용합니다.
	UFUNCTION(BlueprintCallable, Category="Memo")
	bool ConfigureMemoTable(const UDataTable* InTable);

	UFUNCTION(BlueprintPure, Category="Memo")
	EMemoState GetMemoState(FName MemoId) const;

	UFUNCTION(BlueprintPure, Category="Memo")
	bool IsMemoRead(FName MemoId) const;

	UFUNCTION(BlueprintCallable, Category="Memo")
	bool GetMemoData(
		FName MemoId,
		FMemoDefinition& OutData) const;

	// 테이블이나 메모지가 준비되기 전의 사건도 기록합니다.
	UFUNCTION(BlueprintCallable, Category="Memo")
	void ReportMemoEvent(FName EventId);

	// 실제 읽기 화면이 열린 뒤 호출합니다.
	UFUNCTION(BlueprintCallable, Category="Memo")
	bool MarkMemoRead(FName MemoId);

	UPROPERTY(BlueprintAssignable, Category="Memo")
	FMemoStateChanged OnMemoStateChanged;

private:
	UFUNCTION()
	void HandleDayStarted(int32 Day);

	void EvaluateUnlocks();

	UPROPERTY(Transient)
	TObjectPtr<UHotelPhaseSubsystem> Phase;

	UPROPERTY(Transient)
	TObjectPtr<const UDataTable> MemoTable;

	TMap<FName, EMemoState> MemoStates;
	TSet<FName> ReportedEvents;

	int32 CurrentDay = 1;
};
