// Backpacks, Network Prediction half (M11 L7c: the dome blocks hits).

#include "GSTN_HitStep_BackpackDome.h"

#include "GSTN_BackpackDomeOutcome.h"
#include "GSTN_BackpackDomeSnapshot.h"
#include "GSTN_BackpackTypes.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Sync/GSTN_SyncFragment.h"

int32 UGSTN_HitStep_BackpackDome::FindBlockingDome(const FGSTN_HitContextView& View)
{
	// The shell: only an attacker outside reaches it (the attacker tick and the projectile skip it from inside), so it always blocks
	if (GSTNHurtboxIndex::IsExtra(View.HurtboxIndex))
	{
		const int32 Position = GSTNHurtboxIndex::ToExtraPosition(View.HurtboxIndex);
		if (View.Victim.ExtraHurtboxes.IsValidIndex(Position) && View.Victim.ExtraHurtboxes[Position].Source == GSTNFeatureIds::Backpack)
		{
			return View.Victim.EntityId;
		}
	}
	// A victim inside a dome is reached through the body only by an attacker inside the same dome; one that is not in it hit from outside
	const FGSTN_BackpackSnapshotExt* Victim = FGSTN_BackpackSnapshotExt::FromView(View.Victim);
	if (!Victim || Victim->NumInside == 0)
	{
		return INDEX_NONE;
	}
	const FGSTN_BackpackSnapshotExt* Attacker = FGSTN_BackpackSnapshotExt::FromAttacker(View.Incoming);
	for (int32 Index = 0; Index < FMath::Min<int32>(Victim->NumInside, FGSTN_BackpackSnapshotExt::MaxInsideDomes); ++Index)
	{
		const int32 Dome = Victim->InsideDomes[Index];
		if (!Attacker || !Attacker->IsInside(Dome))
		{
			return Dome;
		}
	}
	return INDEX_NONE;
}

EGSTN_StepResult UGSTN_HitStep_BackpackDome::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit)
	{
		return EGSTN_StepResult::Continue;
	}
	const int32 DomeOwner = FindBlockingDome(View);
	if (DomeOwner == INDEX_NONE)
	{
		return EGSTN_StepResult::Continue;
	}
	// The hit did nothing: no damage, no reaction and no hitstop on either side (the melee attacker's recoil is the attacker outcome's)
	Out.bBlocked = true;
	Out.bNoDamage = true;
	Out.bNoFlinch = true;
	Out.bHasKnockback = true;
	Out.Knockback = FGSTN_KnockbackResult();
	Out.Knockback.bStaggered = false;
	Out.Knockback.Reaction = EGSTN_HitReaction::StaggerInPlace;
	Out.Knockback.HitstopFrames = 0;
	Out.Knockback.AttackerHitstopFrames = 0;

	FGSTN_BackpackDomeBlock Block;
	Block.DomeOwnerId = DomeOwner;
	Out.OutcomeExtensions.Add(FInstancedStruct::Make(Block));
	return EGSTN_StepResult::Stop;
}

FName FGSTN_BackpackDomeAttackerOutcome::GetName() const
{
	return TEXT("Backpack.Dome");
}

EGSTN_AttackerHitOutcome FGSTN_BackpackDomeAttackerOutcome::Evaluate(const FGSTN_HitContextView& View) const
{
	for (const FInstancedStruct& Entry : View.OutcomeExtensions)
	{
		if (Entry.GetPtr<FGSTN_BackpackDomeBlock>())
		{
			return EGSTN_AttackerHitOutcome::Ricochet;
		}
	}
	return EGSTN_AttackerHitOutcome::Normal;
}

const UScriptStruct* FGSTN_BackpackDomeEventApplier::GetExtensionStruct() const
{
	return FGSTN_BackpackDomeBlock::StaticStruct();
}

void FGSTN_BackpackDomeEventApplier::AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const
{
	if (!Extension.GetPtr<FGSTN_BackpackDomeBlock>())
	{
		return;
	}
	FGSTN_SimEvent Event = Envelope;
	Event.EventType = GSTNBackpackTags::Event_DomeBlocked;
	Outbox.Add(MoveTemp(Event));
}
