// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Pipeline/GSTN_PartsSnapshotExt.h"

#include "Hits/GSTN_HitVictimView.h"
#include "Pipeline/GSTN_PartHitOutcome.h"

FGSTN_PartsState FGSTN_PartsSnapshotExt::ToState() const
{
	FGSTN_PartsState State;
	State.BrokenMask = BrokenMask;
	State.WoundedMask = WoundedMask;
	FMemory::Memcpy(State.PartDamage, PartDamage, sizeof(PartDamage));
	State.BreakSerial = BreakSerial;
	return State;
}

void FGSTN_PartsSnapshotExt::SetState(const FGSTN_PartsState& State)
{
	BrokenMask = State.BrokenMask;
	WoundedMask = State.WoundedMask;
	FMemory::Memcpy(PartDamage, State.PartDamage, sizeof(PartDamage));
	BreakSerial = State.BreakSerial;
}

FGSTN_PartsSnapshotExt FGSTN_PartsSnapshotExt::FromMinionStore(const UGSTN_PartSetDefinition* InPartSet, uint8 InBrokenMask, const uint8* Damage)
{
	FGSTN_PartsSnapshotExt Ext;
	Ext.PartSet = InPartSet;
	Ext.BrokenMask = InBrokenMask;
	if (Damage)
	{
		FMemory::Memcpy(Ext.PartDamage, Damage, GSTN_PartsLimits::MaxMinionParts);
	}
	return Ext;
}

bool FGSTN_PartsSnapshotExt::ToMinionStore(const FGSTN_PartsState& State, uint8& OutBrokenMask, uint8 (&OutDamage)[GSTN_PartsLimits::MaxMinionParts])
{
	// A wound has no column in the minion row (validation rejects a wounded hardness on a minion set), so a state that carries one does not fit
	bool bFits = (State.BrokenMask >> GSTN_PartsLimits::MaxMinionParts) == 0 && State.WoundedMask == 0;
	for (int32 Index = GSTN_PartsLimits::MaxMinionParts; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		bFits = bFits && State.PartDamage[Index] == 0;
	}
	OutBrokenMask = static_cast<uint8>(State.BrokenMask & 0xFFu);
	FMemory::Memcpy(OutDamage, State.PartDamage, GSTN_PartsLimits::MaxMinionParts);
	return bFits;
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
	State.WoundedMask = ResultWoundedMask;
	FMemory::Memcpy(State.PartDamage, ResultPartDamage, sizeof(ResultPartDamage));
	State.BreakSerial = ResultBreakSerial;
	return State;
}
