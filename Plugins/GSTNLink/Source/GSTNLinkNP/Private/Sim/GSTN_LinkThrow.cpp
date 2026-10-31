// Links, Network Prediction half.

#include "Sim/GSTN_LinkThrow.h"

#include "Data/GSTN_GrabRules.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Events/GSTN_SimEvent.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkWatchdog.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "State/GSTN_LinkMath.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "World/GSTN_StepSnapshot.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLinkThrow, Log, All);

// ---- Thrown side

const UScriptStruct* FGSTN_LinkThrowOutcomeApplier::GetExtensionStruct() const
{
	return FGSTN_LinkThrowOutcome::StaticStruct();
}

void FGSTN_LinkThrowOutcomeApplier::Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const
{
	const FGSTN_LinkThrowOutcome* Outcome = Extension.GetPtr<FGSTN_LinkThrowOutcome>();
	FGSTN_LinkFragment* Fragment = Outcome && Context.VictimFragments ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(*Context.VictimFragments) : nullptr;
	if (!Fragment || !Fragment->Links.Body.MatchesBreak(Outcome->Serial))
	{
		return;
	}
	FGSTN_LinkState& Body = Fragment->Links.Body;
	Body.Reset();
	Body.BreakReason = static_cast<uint8>(EGSTN_LinkBreakReason::Throw);
	Fragment->GrabImmunityEndFrame = FMath::Max(Fragment->GrabImmunityEndFrame, Outcome->ImmunityEndFrame);
	FGSTN_LinkActions::FExit& Exit = Fragment->Actions.Exit;
	Exit.Frame = Outcome->ExitFrame;
	Exit.Position = FVector(Outcome->ExitPosition);
	Exit.YawDeg = Outcome->ExitYawDeg;
	// No velocity of its own: the throw's knockback launches the entity when its hitstop ends, as any hit's does
	Exit.Velocity = FVector::ZeroVector;
	Exit.Flags = GSTNLinkExitFlags::Teleport;
}

void FGSTN_LinkThrowOutcomeApplier::AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const
{
	if (!Extension.GetPtr<FGSTN_LinkThrowOutcome>())
	{
		return;
	}
	FGSTN_SimEvent Event = Envelope;
	Event.EventType = GSTNLinkTags::Event_Throw;
	Outbox.Add(MoveTemp(Event));
	FGSTN_SimEvent Broken = Envelope;
	Broken.EventType = GSTNLinkTags::Event_LinkBroken;
	Broken.Magnitude = static_cast<float>(EGSTN_LinkBreakReason::Throw);
	Broken.Tags.AddTag(GSTNLinkTags::BreakReason_Throw);
	Outbox.Add(MoveTemp(Broken));
}

// ---- Outcome keys

bool FGSTN_LinkOutcomeKeys::GetCrossedFrames(const FGSTN_CombatSyncCore& In, const FGSTN_CombatSyncCore& Out, int32 Frame, int32& OutPrevious, int32& OutCurrent)
{
	if (Out.ActionState != EGSTN_ActionState::InTimeline || Out.TimelineStartFrame == INDEX_NONE || Out.RecoilStartFrame != INDEX_NONE)
	{
		return false;
	}
	OutCurrent = Frame - Out.TimelineStartFrame - Out.TimelinePausedFrames;
	const bool bSameRun = In.ActionState == EGSTN_ActionState::InTimeline && In.TimelineStartFrame == Out.TimelineStartFrame && In.ActiveTimeline == Out.ActiveTimeline;
	// A timeline started by a command at step 8 of the previous frame had its frame 0 evaluated after the hooks of that frame: it is new here
	if (!bSameRun || Out.TimelineStartFrame >= Frame - 1)
	{
		OutPrevious = -1;
	}
	else
	{
		OutPrevious = (Frame - 1) - In.TimelineStartFrame - In.TimelinePausedFrames;
	}
	return OutCurrent > OutPrevious;
}

void FGSTN_LinkOutcomeKeys::Collect(const FGSTN_CombatTimelineData& Data, int32 Previous, int32 Current, TArray<const FGSTN_LinkOutcomeKey*, TInlineAllocator<2>>& OutKeys)
{
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instanced : Data.Tracks)
	{
		const FGSTN_LinkOutcomeTrack* Track = Instanced.GetPtr<FGSTN_LinkOutcomeTrack>();
		if (!Track)
		{
			continue;
		}
		for (const FGSTN_LinkOutcomeKey& Key : Track->Keys)
		{
			const int32 At = Track->StartFrame + Key.Frame;
			if (At > Previous && At <= Current)
			{
				OutKeys.Add(&Key);
			}
		}
	}
}

// ---- Thrower side

FGSTN_HitInboxEntry FGSTN_LinkThrow::MakeThrowEntry(const FGSTN_LinkOutcomeKey& Key, int32 Frame, const FGSTN_SnapshotEntry& Self, const FGSTN_SnapshotEntry& Partner,
	uint8 Serial, uint8 LinkTypeIndex, const UGSTN_CombatRules* Rules)
{
	FGSTN_HitInboxEntry Entry;
	Entry.Frame = Frame;
	Entry.AttackerId = Self.EntityId;
	Entry.AttackerTeam = Self.Team.GetId();
	Entry.Hit = Key.ThrowHit;
	if (!Entry.Hit.HitType.IsValid())
	{
		Entry.Hit.HitType = GSTNLinkTags::HitType_Throw;
	}
	FGSTN_LinkPartnerHit Partnered;
	Partnered.Serial = Serial;
	Partnered.LinkTypeIndex = LinkTypeIndex;
	Entry.Hit.ExtensionData.Add(FInstancedStruct::Make(Partnered));
	Entry.HitLocation = Partner.Transform.GetLocation();
	Entry.HitboxVelocity = FVector::ZeroVector;
	Entry.AttackerYaw = static_cast<float>(Self.Facing.Rotation().Yaw);

	FGSTN_RelationshipQuery Query;
	Query.AttackerEntityId = Self.EntityId;
	Query.AttackerTeamId = Self.Team.GetId();
	Query.VictimEntityId = Partner.EntityId;
	Query.VictimTeamId = Partner.Team.GetId();
	const FGameplayTag Relationship = Rules ? Rules->ResolveRelationship(Query) : GSTNCombatHitTags::Relationship_Enemy.GetTag();
	// A throw of an ally (a co-op carry) is still a throw: the self relationship never happens (one entity cannot hold itself)
	Entry.Relationship = GSTNHitRelationship::ToIndex(Relationship);
	Entry.HurtboxIndex = 0;
	Entry.AttackerHitboxIndex = 0;
	Entry.HurtState = 0;
	return Entry;
}

bool FGSTN_LinkThrow::EndOwnLink(FGSTN_LinkFragment& Fragment, int32 Frame, const FGSTN_GrabRules* Rules)
{
	FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Body.IsActive())
	{
		return false;
	}
	Body.Reset();
	Body.BreakReason = static_cast<uint8>(EGSTN_LinkBreakReason::Throw);
	const int32 Cooldown = Rules ? FMath::Max(Rules->GrabberCooldownFrames, 0) : 0;
	if (Cooldown > 0)
	{
		Fragment.Actions.CooldownEndFrame = FMath::Max(Fragment.Actions.CooldownEndFrame, Frame + Cooldown);
	}
	// No teleport: the thrower stays where it is. The record only dates the reason, which the idle hygiene drops after it
	Fragment.Actions.Exit = FGSTN_LinkActions::FExit();
	Fragment.Actions.Exit.Frame = Frame + 2;
	return true;
}

const FGSTN_GrabRules* FGSTN_LinkThrow::FindGrabRules(const UGSTN_LinkTypeDefinition* Definition, const FGSTN_LinkKitExtension* InitiatorKit)
{
	if (InitiatorKit)
	{
		return &InitiatorKit->GrabRules;
	}
	if (!Definition)
	{
		return nullptr;
	}
	for (const UGSTN_LinkRule* Rule : Definition->Rules)
	{
		if (const UGSTN_LinkRule_Grab* Grab = Cast<UGSTN_LinkRule_Grab>(Rule))
		{
			return &Grab->Rules;
		}
	}
	return nullptr;
}

void FGSTN_LinkThrow::WarnUnresolvedExtensions(const FGSTN_HitData& ThrowHit)
{
	static bool bWarned = false;
	if (bWarned)
	{
		return;
	}
	for (const FInstancedStruct& Extension : ThrowHit.ExtensionData)
	{
		if (!Extension.IsValid())
		{
			// The thrown body extension of the held-objects plugin (M10 6b) with that plugin off: the throw is the plain M6 launch
			bWarned = true;
			UE_LOG(LogGSTNLinkThrow, Warning, TEXT("A throw key's hit carries an extension whose struct is not loaded (the held-objects plugin is off?): ignored, the throw is a plain launch"));
			return;
		}
	}
}
