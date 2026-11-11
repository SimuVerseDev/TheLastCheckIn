#include "Night/Anomaly/AnomalyBase.h"

/*
Anomalies are driven by the manager and state changes, so ticking is disabled.
*/
AAnomalyBase::AAnomalyBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

/*
Starts the anomaly so it becomes visible on CCTV.
Returns false if it is already running.
*/
bool AAnomalyBase::StartAnomaly()
{
	if (!TrySetState(EAnomalyState::Active))
	{
		return false;
	}
	OnAnomalyStarted();
	return true;
}

/*
Escalates an anomaly that was left unresolved for too long (e.g. decoration falls, complaint).
Only allowed once, from the Active state.
*/
bool AAnomalyBase::Escalate()
{
	if (!TrySetState(EAnomalyState::Escalated))
	{
		return false;
	}
	OnAnomalyEscalated();
	return true;
}

/*
Ends the anomaly as unresolved (e.g. the night is over).
*/
bool AAnomalyBase::Fail()
{
	if (!TrySetState(EAnomalyState::Failed))
	{
		return false;
	}
	OnAnomalyFailed();
	return true;
}

/*
Ends the anomaly as solved by the player.
*/
bool AAnomalyBase::Resolve()
{
	if (!TrySetState(EAnomalyState::Resolved))
	{
		return false;
	}
	OnAnomalyResolved();
	return true;
}

/*
True while the anomaly is active or escalated.
*/
bool AAnomalyBase::IsRunning() const
{
	return State == EAnomalyState::Active || State == EAnomalyState::Escalated;
}

/*
Validates the transition, applies it, then notifies listeners.
Returns false and changes nothing if the transition is not allowed.
*/
bool AAnomalyBase::TrySetState(EAnomalyState NewState)
{
	bool bAllowed = false;
	switch (NewState)
	{
	case EAnomalyState::Active:
		// Not running yet, or reused on a later night.
		bAllowed = !IsRunning();
		break;
	case EAnomalyState::Escalated:
		bAllowed = State == EAnomalyState::Active;
		break;
	case EAnomalyState::Resolved:
	case EAnomalyState::Failed:
		bAllowed = IsRunning();
		break;
	default:
		break;
	}

	if (!bAllowed)
	{
		return false;
	}

	const EAnomalyState OldState = State;
	State = NewState;
	OnStateChanged.Broadcast(this, OldState, NewState);
	return true;
}