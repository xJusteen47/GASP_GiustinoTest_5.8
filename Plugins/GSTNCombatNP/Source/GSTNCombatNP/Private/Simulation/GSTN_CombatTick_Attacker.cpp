// Combat simulation (GAS-NPP fork): the attacker side of the combat tick (combat-m4-hits 6, M4 H8).

#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatVictim.h"

#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Rules/GSTN_CombatRules.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "World/GSTN_StepSnapshot.h"

FGameplayTag GSTNHitRelationship::FromIndex(uint8 Index)
{
	switch (Index)
	{
	case 0:
		return GSTNCombatHitTags::Relationship_Self.GetTag();
	case 1:
		return GSTNCombatHitTags::Relationship_Ally.GetTag();
	case 2:
		return GSTNCombatHitTags::Relationship_Enemy.GetTag();
	case 3:
		return GSTNCombatHitTags::Relationship_Neutral.GetTag();
	default:
		return FGameplayTag::EmptyTag;
	}
}

uint8 GSTNHitRelationship::ToIndex(const FGameplayTag& Relationship)
{
	if (Relationship == GSTNCombatHitTags::Relationship_Self.GetTag())
	{
		return 0;
	}
	if (Relationship == GSTNCombatHitTags::Relationship_Ally.GetTag())
	{
		return 1;
	}
	if (Relationship == GSTNCombatHitTags::Relationship_Enemy.GetTag())
	{
		return 2;
	}
	if (Relationship == GSTNCombatHitTags::Relationship_Neutral.GetTag())
	{
		return 3;
	}
	return 255;
}

namespace
{
	/** A hitbox that touches a victim this frame, before the once-per-group and the target limit are applied */
	struct FAttackerCandidate
	{
		int32 EntryIndex = INDEX_NONE;
		int32 HitboxIndex = 0;
		const FGSTN_HitboxSpec* Spec = nullptr;
		FGSTN_HitData Hit;
		FGameplayTag Relationship;
		int32 HurtboxIndex = 0;
		EGSTN_HurtState HurtState = EGSTN_HurtState::Normal;
		FVector HitLocation = FVector::ZeroVector;
		FVector HitboxCenter = FVector::ZeroVector;
		FVector HitboxVelocity = FVector::ZeroVector;
	};

	/** The transform that carries a hitbox's keys and shape at a timeline frame; false when its space cannot be placed (a missing baked socket, a projectile) */
	bool BuildCarrier(const FGSTN_HitboxSpec& Spec, const FGSTN_CombatTimelineData& Timeline, const FGSTN_CombatSyncCore& Core, const FTransform& Root,
		int32 TimelineFrame, int32 TrackFrame, FTransform& OutCarrier)
	{
		const FTransform Key = UGSTN_HitShapeUtils::EvaluateKeys(Spec.Keys, TrackFrame);
		switch (Spec.Space)
		{
		case EGSTN_HitboxSpace::Root:
			OutCarrier = Key * Root;
			return true;
		case EGSTN_HitboxSpace::BakedSocket:
		{
			FTransform Socket;
			if (!Timeline.FindBakedSocket(Spec.Socket, TimelineFrame, Socket))
			{
				return false;
			}
			OutCarrier = Key * Socket * Root;
			return true;
		}
		case EGSTN_HitboxSpace::WorldAtMoveStart:
			// A move whose origin was not written (a timeline adopted before the origin existed) falls back to where the attacker is
			OutCarrier = Key * (Core.Hit.MoveOrigin.bSet ? Core.Hit.MoveOrigin.ToTransform() : Root);
			return true;
		case EGSTN_HitboxSpace::Projectile:
		default:
			// Owned by the projectile system, which does not exist yet: the timeline only spawns it
			return false;
		}
	}

	FVector ShapeCenter(const FGSTN_HitShape& Shape, const FTransform& Carrier)
	{
		return (Shape.Local * Carrier).GetLocation();
	}

	int32 CountTargetsOfGroup(const FGSTN_HitSyncState& Hit, int32 TimelineStartFrame, uint8 HitGroup)
	{
		int32 Count = 0;
		for (int32 Age = 0; Age < Hit.NumHitTargets(); ++Age)
		{
			const FGSTN_HitTarget& Target = Hit.GetHitTarget(Age);
			Count += Target.TimelineStartFrame == TimelineStartFrame && Target.HitGroup == HitGroup ? 1 : 0;
		}
		return Count;
	}
}

void FGSTN_CombatAttacker::Tick(const FGSTN_AttackerTickInput& In, FGSTN_CombatSyncCore& Core, FGSTN_TimelineFrameResult& Scratch, TArray<FGSTN_AttackerHit>& OutHits)
{
	OutHits.Reset();
	if (!In.Timeline || !In.Snapshot || In.AttackerId == INDEX_NONE || Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame == INDEX_NONE)
	{
		return;
	}

	FGSTN_TimelinePlayer::EvaluateHits(*In.Timeline, In.TimelineFrame, Scratch);
	if (Scratch.Hitboxes.IsEmpty())
	{
		return;
	}

	const FGSTN_StepSnapshot& Snapshot = *In.Snapshot;
	const int32 StartFrame = Core.TimelineStartFrame;
	const FVector AttackerFacing = In.Root.GetUnitAxis(EAxis::X);

	// 1 to 4: every open hitbox against every candidate. A victim is hit once per group and execution; several hitboxes of one group that
	// touch the same victim in the same frame leave the one with the highest Data.Priority (ties: the lower index)
	TArray<FAttackerCandidate, TInlineAllocator<8>> Candidates;
	TArray<int32> EntryIndices;
	TArray<int32, TInlineAllocator<8>> Overlapped;

	for (int32 HitboxIndex = 0; HitboxIndex < Scratch.Hitboxes.Num(); ++HitboxIndex)
	{
		const FGSTN_ActiveHitbox& Active = Scratch.Hitboxes[HitboxIndex];
		if (!Active.Spec || In.GroupMask.Has(Active.Spec->HitGroup))
		{
			continue;
		}
		const FGSTN_HitboxSpec& Spec = *Active.Spec;

		FTransform Carrier;
		if (!BuildCarrier(Spec, *In.Timeline, Core, In.Root, In.TimelineFrame, Active.TrackFrame, Carrier))
		{
			continue;
		}
		// The previous carrier moves the keys and the baked frame back one frame and keeps the root: the root of the previous frame is not state
		FTransform PreviousCarrier = Carrier;
		if (!BuildCarrier(Spec, *In.Timeline, Core, In.Root, FMath::Max(In.TimelineFrame - 1, 0), FMath::Max(Active.TrackFrame - 1, 0), PreviousCarrier))
		{
			PreviousCarrier = Carrier;
		}

		if (In.DebugShapes)
		{
			FGSTN_AttackerDebugShape& Debug = In.DebugShapes->AddDefaulted_GetRef();
			Debug.Frame = In.Frame;
			Debug.HitboxIndex = HitboxIndex;
			Debug.Space = static_cast<uint8>(Spec.Space);
			Debug.Shape = Spec.Shape;
			Debug.World = Carrier;
		}

		const FVector HitboxCenter = ShapeCenter(Spec.Shape, Carrier);
		const FVector HitboxVelocity = (HitboxCenter - ShapeCenter(Spec.Shape, PreviousCarrier)) / FMath::Max<double>(In.StepSeconds, UE_SMALL_NUMBER);

		// 2: broadphase. The box holds the shape at the current and the previous carrier, grown by its largest world extent, the offset of the
		// shape inside its carrier and the largest scale a relationship variant gives it
		double VariantScale = 1.0;
		for (const TPair<FGameplayTag, FGSTN_HitVariant>& Pair : Spec.RelationshipVariants)
		{
			VariantScale = FMath::Max<double>(VariantScale, Pair.Value.ShapeScale);
		}
		const double Reach = (Spec.Shape.Extent.GetMax() * FMath::Max(Carrier.GetScale3D().GetAbsMax(), PreviousCarrier.GetScale3D().GetAbsMax())
			+ Spec.Shape.Local.GetLocation().Size()) * VariantScale;
		FBox Bounds(ForceInit);
		Bounds += HitboxCenter;
		Bounds += ShapeCenter(Spec.Shape, PreviousCarrier);
		Bounds = Bounds.ExpandBy(Reach);
		EntryIndices.Reset();
		Snapshot.QueryCandidates(Bounds, EntryIndices);

		for (const int32 EntryIndex : EntryIndices)
		{
			const FGSTN_SnapshotEntry& Victim = Snapshot.Entries[EntryIndex];
			if (!Victim.Hurtboxes || Victim.Hurtboxes->Hurtboxes.IsEmpty())
			{
				continue;
			}

			// 3: relationship, then what the rules let through, then the hitbox's variant
			FGSTN_RelationshipQuery Query;
			Query.AttackerEntityId = In.AttackerId;
			Query.AttackerTeamId = In.AttackerTeam.GetId();
			Query.VictimEntityId = Victim.EntityId;
			Query.VictimTeamId = Victim.Team.GetId();
			const FGameplayTag Relationship = In.Rules ? In.Rules->ResolveRelationship(Query) : UGSTN_RelationshipResolver::DefaultRule(Query);

			const FGSTN_HitVariant* Variant = Spec.RelationshipVariants.Find(Relationship);
			if (Variant && Variant->bIgnore)
			{
				continue;
			}
			if (Relationship == GSTNCombatHitTags::Relationship_Self.GetTag())
			{
				// Self is never hit unless the hitbox says what to do about it
				if (!Variant)
				{
					continue;
				}
			}
			else if (In.Rules ? !In.Rules->IsRelationshipHittable(Relationship)
				: (Relationship != GSTNCombatHitTags::Relationship_Enemy.GetTag() && Relationship != GSTNCombatHitTags::Relationship_Neutral.GetTag()))
			{
				continue;
			}
			if (Core.Hit.HasHitTarget(Victim.EntityId, StartFrame, Spec.HitGroup))
			{
				continue;
			}

			// 4: overlap against the hurtboxes the victim's modifiers leave hittable
			const FGSTN_HitShape HitShape = Variant && !FMath::IsNearlyEqual(Variant->ShapeScale, 1.f) ? UGSTN_HitShapeUtils::ScaleShape(Spec.Shape, Variant->ShapeScale) : Spec.Shape;
			const TArray<FGSTN_Hurtbox>& Hurtboxes = Victim.Hurtboxes->Hurtboxes;
			Overlapped.Reset();
			for (int32 HurtIndex = 0; HurtIndex < Hurtboxes.Num(); ++HurtIndex)
			{
				if (Victim.HurtStates.IsValidIndex(HurtIndex) && Victim.HurtStates[HurtIndex].State == EGSTN_HurtState::Intangible)
				{
					continue;
				}
				const bool bTouches = Spec.bSweepBetweenFrames
					? UGSTN_HitShapeUtils::SweptOverlap(HitShape, PreviousCarrier, Carrier, Hurtboxes[HurtIndex].Shape, Victim.Transform)
					: UGSTN_HitShapeUtils::Overlap(HitShape, Carrier, Hurtboxes[HurtIndex].Shape, Victim.Transform);
				if (bTouches)
				{
					Overlapped.Add(HurtIndex);
				}
			}
			// The extras of the snapshot (a shield bubble) are tested like the body; they have no hurt state, so nothing makes them intangible
			for (int32 ExtraPosition = 0; ExtraPosition < Victim.ExtraHurtboxes.Num(); ++ExtraPosition)
			{
				const FGSTN_HitShape& ExtraShape = Victim.ExtraHurtboxes[ExtraPosition].Shape;
				const bool bTouches = Spec.bSweepBetweenFrames
					? UGSTN_HitShapeUtils::SweptOverlap(HitShape, PreviousCarrier, Carrier, ExtraShape, Victim.Transform)
					: UGSTN_HitShapeUtils::Overlap(HitShape, Carrier, ExtraShape, Victim.Transform);
				if (bTouches)
				{
					Overlapped.Add(GSTNHurtboxIndex::FromExtraPosition(ExtraPosition));
				}
			}
			if (Overlapped.IsEmpty())
			{
				continue;
			}
			// The hit with the variant applied: its type decides which extras let it through, so it is resolved before the pick
			const FGSTN_HitData VariantHit = Variant ? UGSTN_HitDataUtils::ApplyVariant(Spec.Data, *Variant) : Spec.Data;
			const int32 Picked = UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Hurtboxes, Victim.ExtraHurtboxes, VariantHit.HitType);
			const FGSTN_HitShape* PickedShape = Victim.FindHurtboxShape(Picked);
			if (Picked == INDEX_NONE || !PickedShape)
			{
				continue;
			}

			FAttackerCandidate Candidate;
			Candidate.EntryIndex = EntryIndex;
			Candidate.HitboxIndex = HitboxIndex;
			Candidate.Spec = &Spec;
			Candidate.Hit = VariantHit;
			Candidate.Relationship = Relationship;
			Candidate.HurtboxIndex = Picked;
			Candidate.HurtState = Victim.HurtStates.IsValidIndex(Picked) ? Victim.HurtStates[Picked].State : EGSTN_HurtState::Normal;
			Candidate.HitboxCenter = HitboxCenter;
			Candidate.HitLocation = (HitboxCenter + ShapeCenter(*PickedShape, Victim.Transform)) * 0.5;
			Candidate.HitboxVelocity = HitboxVelocity;

			bool bReplaced = false;
			for (FAttackerCandidate& Existing : Candidates)
			{
				if (Existing.EntryIndex == EntryIndex && Existing.Spec->HitGroup == Spec.HitGroup)
				{
					if (Candidate.Hit.Priority > Existing.Hit.Priority)
					{
						Existing = Candidate;
					}
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				Candidates.Add(Candidate);
			}
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	// Victims by id, hitboxes by index: the order every machine writes the ring and the inbox in
	Candidates.Sort([](const FAttackerCandidate& A, const FAttackerCandidate& B)
	{
		return A.EntryIndex != B.EntryIndex ? A.EntryIndex < B.EntryIndex : A.HitboxIndex < B.HitboxIndex;
	});

	// 5 and 6: the new hits and what the attacker does about each
	int32 LongestHitstop = 0;
	float MeterTotal = 0.f;
	for (const FAttackerCandidate& Candidate : Candidates)
	{
		const FGSTN_SnapshotEntry& Victim = Snapshot.Entries[Candidate.EntryIndex];
		const uint8 Group = Candidate.Spec->HitGroup;
		if (CountTargetsOfGroup(Core.Hit, StartFrame, Group) >= FMath::Clamp<int32>(Candidate.Spec->MaxTargetsPerGroup, 1, FGSTN_HitSyncState::MaxHitTargets))
		{
			continue;
		}

		FGSTN_HitResolutionContext Context;
		FGSTN_ResolvedHit& Incoming = Context.Incoming;
		Incoming.Hit = Candidate.Hit;
		Incoming.AttackerEntityId = In.AttackerId;
		Incoming.AttackerTeamId = In.AttackerTeam.GetId();
		// The TeamFilter step refuses Self, and the hitbox's Self variant already decided this hit may happen: the prediction runs it as an
		// ordinary enemy hit. The inbox entry keeps the real relationship
		Incoming.Relationship = Candidate.Relationship == GSTNCombatHitTags::Relationship_Self.GetTag() ? GSTNCombatHitTags::Relationship_Enemy.GetTag() : Candidate.Relationship;
		Incoming.Up = In.Up;
		Incoming.AttackerFacing = AttackerFacing;
		Incoming.Frame = In.Frame;
		const FGSTN_HitShape* VictimShape = Victim.FindHurtboxShape(Candidate.HurtboxIndex);
		const FVector VictimCenter = VictimShape ? ShapeCenter(*VictimShape, Victim.Transform) : Victim.Transform.GetLocation();
		Incoming.HitboxToVictim = (VictimCenter - Candidate.HitboxCenter).GetSafeNormal(UE_SMALL_NUMBER, AttackerFacing);
		Incoming.HitboxVelocity = Candidate.HitboxVelocity;
		// The snapshot does not say whether the victim stands on something; only a Sakurai angle reads it, and the authority recomputes it
		Incoming.bVictimGrounded = false;

		Context.Victim = FGSTN_CombatVictim::MakeVictimView(Victim, In.Rules);

		Context.HurtboxIndex = static_cast<uint8>(Candidate.HurtboxIndex);
		Context.HurtState = Candidate.HurtState;
		if (Victim.HurtStates.IsValidIndex(Candidate.HurtboxIndex))
		{
			Context.ArmorThreshold = Victim.HurtStates[Candidate.HurtboxIndex].ArmorThreshold;
		}
		Context.HurtboxSet = Victim.Hurtboxes;
		if (In.Rules)
		{
			FGSTN_HitPipelineRunner::Run(*In.Rules, Context);
		}
		if (!Context.Outcome.bHit)
		{
			// A step of the rules refused the hit after all: it did not happen
			continue;
		}

		FGSTN_AttackerHit& Hit = OutHits.AddDefaulted_GetRef();
		Hit.VictimId = Victim.EntityId;
		Hit.HitGroup = Group;
		Hit.bHasKnockback = Context.Outcome.bHasKnockback;
		Hit.Knockback = Context.Outcome.Knockback;
		Hit.CueTag = Candidate.Hit.CueTag;
		Hit.HitLocation = Candidate.HitLocation;
		Hit.bReplacedTarget = Core.Hit.AddHitTarget(Victim.EntityId, StartFrame, Group);

		FGSTN_HitContextView ContextView;
		FGSTN_HitPipelineRunner::MakeView(In.Rules, Context, ContextView);
		Hit.bRicochet = FGSTN_AttackerOutcomeRegistry::Get().Evaluate(In.Rules, ContextView) == EGSTN_AttackerHitOutcome::Ricochet;

		if (!Hit.bRicochet)
		{
			// The consequences the attacker predicts: the hit is confirmed (the gate of OnHit cancels) and its own hitstop opens. A parried hit
			// confirms nothing (M6 F7: the punish follows; OnBlock stays false, a blocked hit confirms like a hit)
			if (!Context.Outcome.bParried)
			{
				Core.Hit.HitConfirmFlags |= GetConfirmBit(Group);
			}
			if (Context.Outcome.bHasKnockback)
			{
				Hit.AttackerHitstopFrames = FMath::Max(Context.Outcome.Knockback.AttackerHitstopFrames, 0);
				LongestHitstop = FMath::Max(LongestHitstop, Hit.AttackerHitstopFrames);
			}
			for (const FGSTN_HitRequest& Request : Context.Outcome.Requests)
			{
				if (Request.Kind == EGSTN_HitRequestKind::MeterGain && Request.Target == EGSTN_HitRequestTarget::Attacker)
				{
					Hit.MeterGain += Request.Magnitude;
				}
			}
			MeterTotal += Hit.MeterGain;
		}

		FGSTN_HitInboxEntry& Entry = Hit.Entry;
		Entry.Frame = In.Frame;
		Entry.AttackerId = In.AttackerId;
		Entry.AttackerTeam = In.AttackerTeam.GetId();
		Entry.Hit = Candidate.Hit;
		Entry.HitLocation = Candidate.HitLocation;
		Entry.HitboxVelocity = Candidate.HitboxVelocity;
		Entry.AttackerYaw = static_cast<float>(AttackerFacing.Rotation().Yaw);
		Entry.Relationship = GSTNHitRelationship::ToIndex(Candidate.Relationship);
		Entry.HurtboxIndex = static_cast<uint8>(Candidate.HurtboxIndex);
		Entry.AttackerHitboxIndex = static_cast<uint8>(FMath::Min(Candidate.HitboxIndex, 255));
		Entry.HurtState = static_cast<uint8>(Candidate.HurtState);
		Entry.bRicochet = Hit.bRicochet;
	}

	if (LongestHitstop > 0)
	{
		// Same window the debug button opened: the frame of the hit runs on, the next ones stand still, and the Mover half is a freeze request
		Core.HitstopStartFrame = In.Frame;
		Core.HitstopEndFrame = In.Frame + LongestHitstop;
		Core.Requests.Add(EGSTN_RequestType::Freeze, In.Frame, static_cast<int16>(FMath::Min(LongestHitstop, 32767)));
	}
	if (MeterTotal > 0.f)
	{
		Core.Requests.Add(EGSTN_RequestType::MeterGain, In.Frame, static_cast<int16>(FMath::Clamp(FMath::RoundToInt(MeterTotal), 1, 32767)));
	}
}
