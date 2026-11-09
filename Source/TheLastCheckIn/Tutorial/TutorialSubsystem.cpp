#include "Tutorial/TutorialSubsystem.h"
#include "Core/GameFlow/HotelPhaseSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogTutorial, Log, All);

void UTutorialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Phase = Collection.InitializeDependency<UHotelPhaseSubsystem>();
	Phase->OnDayStarted.AddDynamic(this, &ThisClass::HandleDayStarted);
}

void UTutorialSubsystem::Deinitialize()
{
	if (Phase) Phase->OnDayStarted.RemoveAll(this);
	OnStepChanged.Clear();
	Phase = nullptr;
	Super::Deinitialize();
}

bool UTutorialSubsystem::IsTutorialActive() const
{
	return Phase && Phase->GetCurrentDay() == 1
		&& CurrentStep != ETutorialStep::NotStarted
		&& CurrentStep != ETutorialStep::Completed;
}

void UTutorialSubsystem::HandleDayStarted(int32 Day)
{
	if (Day == 1)
	{
		// StartRun currently always creates a new run. Future load code must restore
		// tutorial state separately instead of invoking this new-day initialization.
		SetStep(ETutorialStep::CleanRoom);
	}
	else if (!IsTutorialCompleted())
	{
		// Automatic settlement may end day one before the tutorial finishes.
		// Hide the unfinished objective without marking it successfully completed.
		SetStep(ETutorialStep::NotStarted);
	}
}

bool UTutorialSubsystem::CompleteStep(ETutorialStep CompletedStep)
{
	if (!IsTutorialActive() || CompletedStep != CurrentStep) return false;

	ETutorialStep NextStep;
	switch (CurrentStep)
	{
	case ETutorialStep::CleanRoom: NextStep = ETutorialStep::UnlockRoom205; break;
	case ETutorialStep::UnlockRoom205: NextStep = ETutorialStep::CheckInGuest; break;
	case ETutorialStep::CheckInGuest: NextStep = ETutorialStep::AnomalyPresentation; break;
	case ETutorialStep::AnomalyPresentation: NextStep = ETutorialStep::ReadBoardMemo; break;
	case ETutorialStep::ReadBoardMemo: NextStep = ETutorialStep::ViewCCTV; break;
	case ETutorialStep::ViewCCTV: NextStep = ETutorialStep::Sleep; break;
	case ETutorialStep::Sleep: NextStep = ETutorialStep::Completed; break;
	default: return false;
	}

	SetStep(NextStep);
	return true;
}

void UTutorialSubsystem::SetStep(ETutorialStep NewStep)
{
	if (CurrentStep == NewStep) return;
	const ETutorialStep OldStep = CurrentStep;
	CurrentStep = NewStep;
	UE_LOG(LogTutorial, Log, TEXT("Tutorial: %s -> %s"),
		*UEnum::GetValueAsString(OldStep), *UEnum::GetValueAsString(NewStep));
	OnStepChanged.Broadcast(OldStep, NewStep);
}
