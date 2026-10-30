// Defence, Network Prediction half.

#include "GSTN_DefenceOutcomeApplier.h"
#include "Events/GSTN_SimEvent.h"
#include "GSTN_DefenceTags.h"
#include "Pipeline/GSTN_DefenceHitOutcome.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceInboxKinds.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

const UScriptStruct* FGSTN_DefenceOutcomeApplier::GetExtensionStruct() const
{
	return FGSTN_DefenceHitOutcome::StaticStruct();
}

void FGSTN_DefenceOutcomeApplier::Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const
{
	const FGSTN_DefenceHitOutcome* Outcome = Extension.GetPtr<FGSTN_DefenceHitOutcome>();
	if (!Outcome || !Context.VictimFragments || Outcome->Result == EGSTN_DefenceHitResult::None)
	{
		return;
	}
	FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(*Context.VictimFragments);
	const int32 HitFrame = Outcome->HitFrame;
	const int32 HitstopEnd = Outcome->GetHitstopEndFrame();
	switch (Outcome->Result)
	{
	case EGSTN_DefenceHitResult::Parried:
		Fragment.LastParryFrame = FMath::Max(Fragment.LastParryFrame, HitFrame);
		if (Outcome->Source == EGSTN_ParrySource::Shield)
		{
			Fragment.ParryRecoveryEndFrame = FMath::Max(Fragment.ParryRecoveryEndFrame, HitstopEnd + Outcome->DefenderRecoveryFrames);
		}
		else if (Outcome->MoveWindowEndFrame != INDEX_NONE && Fragment.HasMoveParryWindow())
		{
			Fragment.MoveParryEndFrame = FMath::Min(Fragment.MoveParryEndFrame, Outcome->MoveWindowEndFrame);
		}
		break;

	case EGSTN_DefenceHitResult::Blocked:
		// The value at the hit frame, lower; the drain goes on from it (the bubble keeps its size)
		Fragment.ShieldValueQ = Outcome->ShieldValueAfterQ;
		Fragment.ShieldStampFrame = HitFrame;
		Fragment.ShieldStunEndFrame = FMath::Max(Fragment.ShieldStunEndFrame, HitstopEnd + Outcome->StunFrames);
		if (Outcome->PushbackFrames > 0)
		{
			Fragment.SetPushback(HitstopEnd, Outcome->Velocity, Outcome->PushbackFrames);
		}
		// A re-stamp: the tick extension mirrors it on its next tick
		Fragment.MirrorFrame = INDEX_NONE;
		break;

	case EGSTN_DefenceHitResult::Broken:
		Fragment.ShieldValueQ = 0;
		Fragment.ShieldStampFrame = HitFrame;
		Fragment.ShieldFlags = (Fragment.ShieldFlags | GSTNShieldFlags::Breaking) & ~(GSTNShieldFlags::Raised | GSTNShieldFlags::Empty);
		Fragment.ShieldReleaseFrame = HitFrame;
		Fragment.BreakEndFrame = HitFrame + Outcome->StunFrames;
		Fragment.ShieldStunEndFrame = INDEX_NONE;
		Fragment.ClearPushback();
		Fragment.MirrorFrame = INDEX_NONE;
		// Up and away, landing on its feet: a launch that never tumbles
		Context.Reaction.Merge(0, Outcome->Velocity, true);
		break;

	default:
		break;
	}
}

void FGSTN_DefenceOutcomeApplier::AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const
{
	const FGSTN_DefenceHitOutcome* Outcome = Extension.GetPtr<FGSTN_DefenceHitOutcome>();
	if (!Outcome)
	{
		return;
	}
	FGSTN_SimEvent Event = Envelope;
	switch (Outcome->Result)
	{
	case EGSTN_DefenceHitResult::Parried:
		Event.EventType = GSTNDefenceTags::Event_Parried;
		Event.Magnitude = static_cast<float>(static_cast<uint8>(Outcome->Source));
		break;
	case EGSTN_DefenceHitResult::Blocked:
		Event.EventType = GSTNDefenceTags::Event_Blocked;
		break;
	case EGSTN_DefenceHitResult::Broken:
		Event.EventType = GSTNDefenceTags::Event_ShieldBroken;
		break;
	default:
		return;
	}
	Outbox.Add(MoveTemp(Event));
}

// ---- Punish inbox kind

FGSTN_InboxEntry FGSTN_DefencePunishHandler::MakeEntry(const FGSTN_DefencePunishEntry& Payload)
{
	FGSTN_InboxEntry Entry;
	Entry.Kind = GSTNDefenceInboxKinds::Punish;
	Entry.Payload = FInstancedStruct::Make(Payload);
	return Entry;
}

void FGSTN_DefencePunishHandler::Handle(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
{
	const FGSTN_DefencePunishEntry* Punish = Entry.Payload.GetPtr<FGSTN_DefencePunishEntry>();
	if (!Punish || !Context.Sync || Punish->HitFrame == INDEX_NONE)
	{
		return;
	}
	FGSTN_CombatSyncState& Sync = *Context.Sync;
	FGSTN_CombatSyncCore& Core = Sync.Core;
	const int32 Frame = Context.Frame;
	if (Core.ActionState == EGSTN_ActionState::KO)
	{
		return;
	}
	const int32 HitstopEnd = Punish->GetHitstopEndFrame();
	FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Sync.Fragments);
	Fragment.PunishEndFrame = FMath::Max(Fragment.PunishEndFrame, FMath::Max(HitstopEnd, Frame) + Punish->StunFrames);
	Fragment.PunishKind = Punish->Kind;

	// The parried move ends here (hitstun and tumble already ended it; nothing else runs a timeline)
	if (Core.ActionState == EGSTN_ActionState::InTimeline)
	{
		FGSTN_CombatTimelineLogic::CancelForOtherCommand(Core, Frame);
	}
	// The parry hitstop the attacker predicted from the snapshot; where it did not (the snapshots disagreed), it starts now
	if (HitstopEnd > Frame && Core.HitstopEndFrame < HitstopEnd)
	{
		if (!Core.IsInHitstop(Frame))
		{
			Core.HitstopStartFrame = Frame;
		}
		Core.HitstopEndFrame = HitstopEnd;
		Core.Requests.Add(EGSTN_RequestType::Freeze, Frame, static_cast<int16>(FMath::Min(HitstopEnd - Frame, 32767)));
	}
}

bool FGSTN_DefencePunishHandler::Register()
{
	return FGSTN_InboxKindRegistry::Get().Register(GSTNDefenceInboxKinds::Punish, TEXT("Defence.Punish"), &FGSTN_DefencePunishHandler::Handle);
}

void FGSTN_DefencePunishHandler::Unregister()
{
	FGSTN_InboxKindRegistry::Get().Unregister(GSTNDefenceInboxKinds::Punish);
}
