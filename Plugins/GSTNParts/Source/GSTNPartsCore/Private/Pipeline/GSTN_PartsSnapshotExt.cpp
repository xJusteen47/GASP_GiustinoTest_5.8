// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Pipeline/GSTN_PartsSnapshotExt.h"

#include "Hits/GSTN_HitVictimView.h"
#include "Pipeline/GSTN_PartHitOutcome.h"

FGSTN_PartsState FGSTN_PartsSnapshotExt::ToState() const
{
	FGSTN_PartsState State;
	State.BrokenMask = BrokenMask;
	FMemory::Memcpy(State.PartDamage, PartDamage, sizeof(PartDamage));
	State.BreakSerial = BreakSerial;
	return State;
}

void FGSTN_PartsSnapshotExt::SetState(const FGSTN_PartsState& State)
{
	BrokenMask = State.BrokenMask;
	FMemory::Memcpy(PartDamage, State.PartDamage, sizeof(PartDamage));
	BreakSerial = State.BreakSerial;
}

const FGSTN_PartsSnapshotExt* FGSTN_PartsSnapshotExt::FromView(const FGSTN_HitVictimView& Victim)
{
	const FGSTN_PartsSnapshotExt* Ext = static_cast<const FGSTN_PartsSnapshotExt*>(Victim.SnapshotExtension);
	return Ext && Ext->Type == TypeWord ? Ext : nullptr;
}

FGSTN_PartsState FGSTN_PartHitOutcome::GetResultState() const
{
	FGSTN_PartsState State;
	State.BrokenMask = ResultBrokenMask;
	FMemory::Memcpy(State.PartDamage, ResultPartDamage, sizeof(ResultPartDamage));
	State.BreakSerial = ResultBreakSerial;
	return State;
}
