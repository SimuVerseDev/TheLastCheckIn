#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TutorialSubsystem.generated.h"

class UHotelPhaseSubsystem;
class AHKRoomVolume;
class ARoomActor;

UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	NotStarted,
	CleanRoom,
	UnlockRoom205,
	CheckInGuest,
	AnomalyPresentation,
	ReadBoardMemo,
	ViewCCTV,
	Sleep,
	Completed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTutorialStepChanged,
	ETutorialStep, OldStep, ETutorialStep, NewStep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTutorialProgressChanged);

// Prepares the first-day rooms and tracks objectives. Gameplay actors report
// successful actions; this subsystem does not advance the game clock.
UCLASS()
class THELASTCHECKIN_API UTutorialSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	ETutorialStep GetCurrentStep() const { return CurrentStep; }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsTutorialActive() const;

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsTutorialCompleted() const { return CurrentStep == ETutorialStep::Completed; }

	// Call only after the intended tutorial action succeeds (including target checks).
	// Passing the completed step prevents duplicate or out-of-order notifications
	// from advancing a different objective. Returns true only when it advances.
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	bool CompleteStep(ETutorialStep CompletedStep);
	UFUNCTION(BlueprintPure, Category="Tutorial")
	int32 GetCleanedRoomCount() const { return CleanedRooms.Num(); }
	UFUNCTION(BlueprintPure, Category="Tutorial")
	bool IsRoomCleaningCompleted(int32 RoomNumber) const { return CleanedRooms.Contains(RoomNumber); }
	UFUNCTION(BlueprintPure, Category="Tutorial")
	int32 GetCheckedInTeamCount() const { return CheckedInStays.Num(); }
	UFUNCTION(BlueprintPure, Category="Tutorial")
	FText GetObjectiveText() const;
	UPROPERTY(BlueprintAssignable, Category="Tutorial")
	FTutorialProgressChanged OnProgressChanged;

	// Bind before the day starts, or read GetCurrentStep after binding to sync UI.
	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FTutorialStepChanged OnStepChanged;

private:
	UFUNCTION()
	void HandleDayStarted(int32 Day);
	void PrepareInitialRoomDirt();
	void UnbindGameplayEvents();
	void EvaluateObjectives();
	UFUNCTION() void HandleRoomCleaned(AHKRoomVolume* Volume, float Cleanliness);
	UFUNCTION() void HandleRoomPurchased(ARoomActor* Room);
	UFUNCTION() void HandleRoomOccupied(ARoomActor* Room, FGuid StayId);
	void SetStep(ETutorialStep NewStep);

	UPROPERTY(Transient)
	TObjectPtr<UHotelPhaseSubsystem> Phase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tutorial", meta = (AllowPrivateAccess = "true"))
	ETutorialStep CurrentStep = ETutorialStep::NotStarted;
	TArray<TWeakObjectPtr<AHKRoomVolume>> CleaningVolumes;
	TArray<TWeakObjectPtr<ARoomActor>> ObservedRooms;
	TSet<int32> CleanedRooms;
	TSet<FGuid> CheckedInStays;
	bool bRoom205Purchased = false;
};
