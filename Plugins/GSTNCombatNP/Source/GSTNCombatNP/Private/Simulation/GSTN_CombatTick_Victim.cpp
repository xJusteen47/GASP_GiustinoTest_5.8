// Combat simulation (GAS-NPP fork): the victim side of the combat tick (combat-m4-hits 7, M4 H9).

#include "Simulation/GSTN_CombatVictim.h"

#include "AbilitySystemComponent.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

namespace
{
	FVector ShapeCenterOf(const FGSTN_HitShape& Shape, const FTransform& Carrier)
	{
		return (Shape.Local * Carrier).GetLocation();
	}
}

FGSTN_HitVictimView FGSTN_CombatVictim::MakeVictimView(const FGSTN_SnapshotEntry& Victim, const UGSTN_CombatRules* Rules)
{
	FGSTN_HitVictimView View;
	View.EntityId = Victim.EntityId;
	View.TeamId = Victim.Team.GetId();
	View.Location = Victim.Transform.GetLocation();
	View.Velocity = Victim.Velocity;
	View.FacingYaw = Victim.Facing.Rotation().Yaw;
	View.bInHitstop = Victim.bInHitstop;
	View.ReactionState = Victim.Reaction;
	View.HealthFraction = Victim.HealthFraction;
	if (Rules && Rules->DamageMode != EGSTN_DamageMode::Health)
	{
		// The percent modes derive the fraction from the damage taken
		View.HealthFraction = FMath::Clamp(1.f - Rules->GetDamageProgress(View), 0.f, 1.f);
	}
	return View;
}

float FGSTN_CombatVictim::ComputeWeightScale(const UGSTN_CombatRules* Rules, const UGSTN_HurtboxSetDefinition* Hurtboxes, double DamagePercent, float HealthFraction)
{
	if (!Rules)
	{
		return 1.f;
	}
	const FGSTN_BakedCurve* Curve = nullptr;
	if (Hurtboxes && Hurtboxes->WeightCurve.IsAuthored())
	{
		Curve = &Hurtboxes->WeightCurve;
	}
	else if (Rules->DefaultWeightCurve.IsAuthored())
	{
		Curve = &Rules->DefaultWeightCurve;
	}
	if (!Curve)
	{
		return 1.f;
	}
	FGSTN_HitVictimView View;
	View.ReactionState = FGSTN_VictimReactionState::Make(DamagePercent, 1.0, 0, FGSTN_StaggerThreshold());
	View.HealthFraction = HealthFraction;
	return Curve->Evaluate(Rules->GetDamageProgress(View));
}

FVector FGSTN_CombatVictim::StickToPlane(const FVector2D& Stick, double CameraYawDegrees, const FVector& Up)
{
	const FVector UpUnit = Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector Forward = UGSTN_HitMathUtils::ProjectOnPlane(FVector::ForwardVector, UpUnit).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = UGSTN_HitMathUtils::ProjectOnPlane(FVector::RightVector, UpUnit).GetSafeNormal();
	}
	const FQuat Turn(UpUnit, FMath::DegreesToRadians(CameraYawDegrees));
	Forward = Turn.RotateVector(Forward);
	const FVector Right = FVector::CrossProduct(UpUnit, Forward);
	return Forward * Stick.Y + Right * Stick.X;
}

void FGSTN_CombatVictim::Resolve(const FGSTN_VictimTickInput& In, FGSTN_CombatSyncCore& Core, FGSTN_VictimResult& Out)
{
	Out.Hits.Reset();
	Out.FreezeFrames = 0;
	Out.bFreezeZeroVelocity = false;
	Out.bCancelledMove = false;
	Out.KO = EGSTN_KOCause::None;
	Out.KOHitIndex = INDEX_NONE;
	// Knocked out is terminal: nothing resolves any more
	if (!In.Self || In.Entries.IsEmpty() || Core.ActionState == EGSTN_ActionState::KO)
	{
		return;
	}

	const FGSTN_SnapshotEntry& Self = *In.Self;
	const UGSTN_CombatRules* Rules = In.Rules;
	const int32 Frame = In.Frame;

	// The victim the pipeline reads: the snapshot's, moved on by each landed entry so the next one sees the damage the first one did
	FGSTN_HitVictimView VictimView = MakeVictimView(Self, Rules);

	const int32 WindowEndBefore = Core.IsInHitstop(Frame) ? Core.HitstopEndFrame : INDEX_NONE;
	bool bWindowChanged = false;
	bool bAnyStaggered = false;
	int32 LastHitstunFrames = 0;
	EGSTN_HitReaction LastReaction = EGSTN_HitReaction::StaggerInPlace;

	for (const FGSTN_HitInboxEntry& Entry : In.Entries)
	{
		FGSTN_VictimHit& Hit = Out.Hits.AddDefaulted_GetRef();
		Hit.Entry = Entry;
		Hit.bRicochet = Entry.bRicochet;
		if (Entry.bRicochet || !Rules)
		{
			// A ricochet still informs the victim for its cue and event, nothing else; without rules nothing says what a hit does
			continue;
		}

		FGSTN_HitResolutionContext Context;
		FGSTN_ResolvedHit& Incoming = Context.Incoming;
		Incoming.Hit = Entry.Hit;
		Incoming.AttackerEntityId = Entry.AttackerId;
		Incoming.AttackerTeamId = Entry.AttackerTeam;
		// The attacker predicted a Self hit as an ordinary enemy hit (TeamFilter refuses Self): the victim runs the same pipeline on the same input
		const FGameplayTag Relationship = GSTNHitRelationship::FromIndex(Entry.Relationship);
		Incoming.Relationship = Relationship == GSTNCombatHitTags::Relationship_Self.GetTag() ? GSTNCombatHitTags::Relationship_Enemy.GetTag() : Relationship;
		Incoming.Up = Self.Up;
		Incoming.AttackerFacing = FRotator(0.0, Entry.AttackerYaw, 0.0).Vector();
		Incoming.HitboxVelocity = Entry.HitboxVelocity;
		Incoming.bVictimGrounded = In.bGrounded;
		FVector VictimCenter = Self.Transform.GetLocation();
		if (Self.Hurtboxes && Self.Hurtboxes->Hurtboxes.IsValidIndex(Entry.HurtboxIndex))
		{
			VictimCenter = ShapeCenterOf(Self.Hurtboxes->Hurtboxes[Entry.HurtboxIndex].Shape, Self.Transform);
		}
		// HitLocation is the middle of the two centres (the attacker wrote it so), which makes this the direction the attacker computed
		Incoming.HitboxToVictim = (VictimCenter - Entry.HitLocation).GetSafeNormal(UE_SMALL_NUMBER, Incoming.AttackerFacing);

		Context.Victim = VictimView;
		Context.HurtboxIndex = Entry.HurtboxIndex;
		Context.HurtState = static_cast<EGSTN_HurtState>(FMath::Min<uint8>(Entry.HurtState, static_cast<uint8>(EGSTN_HurtState::Armor)));
		if (Self.HurtStates.IsValidIndex(Entry.HurtboxIndex))
		{
			Context.ArmorThreshold = Self.HurtStates[Entry.HurtboxIndex].ArmorThreshold;
		}
		Context.HurtboxSet = Self.Hurtboxes;
		FGSTN_HitPipelineRunner::Run(*Rules, Context);
		if (!Context.Outcome.bHit)
		{
			// A step of the rules refused the hit after all (the attacker's own run had the same answer, so this is a rare disagreement)
			Out.Hits.Pop();
			continue;
		}

		FGSTN_HitPipelineRunner::MakeView(Rules, Context, Hit.View);
		Hit.Outcome = Context.Outcome;

		// The registered outcome appliers, for staggered and unstaggered hits alike; what they return joins the outcome
		FGSTN_OutcomeApplyContext ApplyContext;
		ApplyContext.VictimFragments = In.Fragments;
		FGSTN_OutcomeApplierRegistry::Get().ApplyAll(Rules, Context.OutcomeExtensions, Hit.View, ApplyContext);
		Hit.Outcome.Events.Append(ApplyContext.Result.Events);
		Hit.Outcome.Effects.Append(ApplyContext.Result.Effects);

		const FGSTN_KnockbackResult& Knockback = Hit.Outcome.Knockback;
		Hit.bStaggered = Hit.Outcome.bHasKnockback && Knockback.bStaggered;
		Hit.Reaction = Knockback.Reaction;
		Hit.bTumble = Knockback.bTumble;
		Hit.HitstunFrames = Knockback.HitstunFrames;
		Hit.HitstopFrames = Hit.Outcome.bHasKnockback ? FMath::Max(Knockback.HitstopFrames, 0) : 0;
		FVector LaunchVelocity = Knockback.LaunchVelocity;

		if (Hit.bStaggered)
		{
			if (Hit.Reaction == EGSTN_HitReaction::PushBack && (!In.bGrounded || In.bOnWall))
			{
				// Nothing to slide on: the push becomes a launch along the hit's own direction
				Hit.bPromoted = true;
				Hit.Reaction = EGSTN_HitReaction::Launch;
				const FGSTN_ResolvedHit& Resolved = Context.Incoming;
				const FVector Direction = UGSTN_HitMathUtils::LaunchDirection(Resolved.Hit.YawSource, Resolved.Hit.AngleDeg, Resolved.Hit.SpecialAngle, Resolved.Up,
					Resolved.AttackerFacing, Resolved.HitboxToVictim, Resolved.HitboxVelocity, In.bGrounded, Knockback.Knockback, Rules->Sakurai);
				LaunchVelocity = Direction * Knockback.Knockback;
				const UGSTN_KnockbackModel* Model = Rules->KnockbackModel.Get();
				Hit.bTumble = Model && Knockback.Knockback >= Model->Tuning.TumbleThreshold;
			}
			if (Hit.Reaction != EGSTN_HitReaction::Launch)
			{
				// The locked part of the reaction is never shorter than the floor of the rules
				Hit.HitstunFrames = FMath::Max(Hit.HitstunFrames, Rules->MinStaggerLockFrames);
			}
			Hit.bTumble &= Hit.Reaction == EGSTN_HitReaction::Launch;
		}

		const bool bInHitstunNow = IsActionLocked(Core);
		if (Hit.bStaggered)
		{
			if (Core.ActionState == EGSTN_ActionState::InTimeline)
			{
				FGSTN_CombatTimelineLogic::CancelForOtherCommand(Core, Frame);
				Out.bCancelledMove = true;
			}
			Core.ActionState = Hit.bTumble ? EGSTN_ActionState::Tumble : EGSTN_ActionState::Hitstun;
			Core.Hit.SetPendingReaction(Hit.Reaction);
			Core.Hit.SetPendingLaunch(Hit.Reaction == EGSTN_HitReaction::StaggerInPlace ? FVector::ZeroVector : LaunchVelocity);
			bAnyStaggered = true;
			LastHitstunFrames = Hit.HitstunFrames;
			LastReaction = Hit.Reaction;
		}
		// A hit below the threshold on a victim already in hitstun changes nothing (assumption A-27); damage, effects, cue and event still go out
		const bool bOpensWindow = Hit.HitstopFrames > 0 && (Hit.bStaggered || !bInHitstunNow);
		if (bOpensWindow)
		{
			const int32 End = Entry.Frame + Hit.HitstopFrames;
			if (Core.IsInHitstop(Frame) || bWindowChanged)
			{
				Core.HitstopEndFrame = FMath::Max(Core.HitstopEndFrame, End);
			}
			else
			{
				Core.HitstopStartFrame = Entry.Frame;
				Core.HitstopEndFrame = End;
			}
			bWindowChanged = true;
		}

		FGSTN_HitPipelineRunner::AdvanceVictim(*Rules, Hit.Outcome, VictimView);
		Hit.DamagePercentAfter = VictimView.ReactionState.GetDamagePercent();
		Hit.HealthFractionAfter = VictimView.HealthFraction;
	}

	// The cap: the first hit that left the victim at it knocks it out. The reaction that hit chose still plays (a KO flies away), so only
	// the action state is overwritten, after the reaction was written; KO is the state the hitstun end cannot leave
	for (int32 Index = 0; Index < Out.Hits.Num() && Out.KO == EGSTN_KOCause::None; ++Index)
	{
		if (ReachedCap(Rules, Out.Hits[Index]))
		{
			Out.KO = EGSTN_KOCause::Health;
			Out.KOHitIndex = Index;
		}
	}
	if (Out.KO == EGSTN_KOCause::Health)
	{
		if (Core.ActionState == EGSTN_ActionState::InTimeline)
		{
			FGSTN_CombatTimelineLogic::CancelForOtherCommand(Core, Frame);
			Out.bCancelledMove = true;
		}
		Core.ActionState = EGSTN_ActionState::KO;
	}

	if (bAnyStaggered)
	{
		// A stagger without a hitstop window starts its hitstun now
		const int32 Base = Core.HitstopEndFrame > Frame ? Core.HitstopEndFrame : Frame;
		Core.Hit.HitstunEndFrame = Base + LastHitstunFrames;
		Out.bFreezeZeroVelocity = LastReaction == EGSTN_HitReaction::StaggerInPlace;
	}
	if (bWindowChanged)
	{
		// The window opens on the frame of the hit and the freeze starts now, so both end together. A window that already covered the end
		// (the attacker's own, in a trade) needs no second freeze: two would restore the velocity of the first
		if (Core.HitstopEndFrame > WindowEndBefore)
		{
			Out.FreezeFrames = FMath::Max(Core.HitstopEndFrame - Frame, 0);
		}
	}
}

bool FGSTN_CombatVictim::ApplyZoneKO(FGSTN_CombatSyncCore& Core, int32 Frame, bool bInZone, bool& bCancelledMove)
{
	if (!bInZone || Core.ActionState == EGSTN_ActionState::KO)
	{
		return false;
	}
	if (Core.ActionState == EGSTN_ActionState::InTimeline)
	{
		FGSTN_CombatTimelineLogic::CancelForOtherCommand(Core, Frame);
		bCancelledMove = true;
	}
	Core.ActionState = EGSTN_ActionState::KO;
	return true;
}

bool FGSTN_CombatVictim::ReachedCap(const UGSTN_CombatRules* Rules, const FGSTN_VictimHit& Hit)
{
	if (!Rules || Hit.bRicochet || !Hit.Outcome.bHit)
	{
		return false;
	}
	switch (Rules->DamageMode)
	{
	case EGSTN_DamageMode::Health:
		return Hit.HealthFractionAfter <= 0.f;
	case EGSTN_DamageMode::PercentCapped:
		return Rules->MaxHealth > 0.f && Hit.DamagePercentAfter >= Rules->MaxHealth;
	default:
		return false;
	}
}

void FGSTN_CombatVictim::AppendHitEvents(const FGSTN_VictimHit& Hit, int32 Frame, AActor* Attacker, AActor* Victim, const FGameplayTagContainer& MoveTags, FGSTN_SimEventOutbox& Outbox)
{
	FGSTN_SimEvent Event;
	Event.Frame = Frame;
	Event.Instigator = Attacker ? Attacker : Victim;
	Event.Target = Victim;
	Event.Magnitude = Hit.Entry.Hit.Damage;
	Event.Tags.AppendTags(MoveTags);
	Event.Tags.AddTag(Hit.Entry.Hit.IsNormalHitType() ? GSTNCombatHitTags::HitType_Normal.GetTag() : Hit.Entry.Hit.HitType);
	const FGameplayTag Relationship = GSTNHitRelationship::FromIndex(Hit.Entry.Relationship);
	if (Relationship.IsValid())
	{
		Event.Tags.AddTag(Relationship);
	}
	if (Hit.bStaggered)
	{
		Event.Tags.AddTag(GSTNCombatNPTags::EventHit_Staggered);
		switch (Hit.Reaction)
		{
		case EGSTN_HitReaction::PushBack: Event.Tags.AddTag(GSTNCombatNPTags::EventHit_Reaction_PushBack); break;
		case EGSTN_HitReaction::Launch: Event.Tags.AddTag(GSTNCombatNPTags::EventHit_Reaction_Launch); break;
		default: Event.Tags.AddTag(GSTNCombatNPTags::EventHit_Reaction_StaggerInPlace); break;
		}
		if (Hit.bTumble)
		{
			Event.Tags.AddTag(GSTNCombatNPTags::EventHit_Tumble);
		}
	}

	FGSTN_SimEvent Landed = Event;
	Landed.EventType = GSTNCombatTags::Event_HitLanded;
	Outbox.Add(MoveTemp(Landed));
	if (!Hit.bRicochet)
	{
		Event.EventType = GSTNCombatTags::Event_HitReceived;
		Outbox.Add(MoveTemp(Event));
	}
}

void FGSTN_CombatVictim::ProjectStateTags(UAbilitySystemComponent& ASC, const FGSTN_CombatSyncCore& Core, int32 Frame)
{
	// Set, not added: nothing else grants these three, and a re-simulated frame must not inherit what the predicted future left behind
	ASC.SetLooseGameplayTagCount(GSTNCombatTags::State_Hitstun, Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Splat ? 1 : 0);
	ASC.SetLooseGameplayTagCount(GSTNCombatTags::State_Tumble, Core.ActionState == EGSTN_ActionState::Tumble ? 1 : 0);
	ASC.SetLooseGameplayTagCount(GSTNCombatTags::State_Hitstop, Core.IsInHitstop(Frame) ? 1 : 0);

	// The reaction lock has a tag of its own, set from the state like the others: a member remembering "I hold the lock" is outside the rolled
	// back state, so a resimulation from before the hit would skip the Add. Lock_Movement stays for whoever else roots the combatant
	ASC.SetLooseGameplayTagCount(GSTNCombatTags::Lock_Reaction, IsActionLocked(Core) ? 1 : 0);
}

void FGSTN_CombatVictim::AppendKOEvent(EGSTN_KOCause Cause, int32 Frame, AActor* Attacker, AActor* Victim, FGSTN_SimEventOutbox& Outbox)
{
	FGSTN_SimEvent Event;
	Event.EventType = GSTNCombatTags::Event_KO;
	Event.Frame = Frame;
	Event.Instigator = Cause == EGSTN_KOCause::Health && Attacker ? Attacker : Victim;
	Event.Target = Victim;
	Event.Tags.AddTag(Cause == EGSTN_KOCause::Zone ? GSTNCombatTags::KOReason_Zone : GSTNCombatTags::KOReason_Health);
	Outbox.Add(MoveTemp(Event));
}

FGSTN_ReactionMove FGSTN_CombatVictim::StepReaction(const FGSTN_ReactionStepInput& In, FGSTN_CombatSyncCore& Core)
{
	FGSTN_ReactionMove Move;
	if (!IsActionLocked(Core))
	{
		return Move;
	}
	const int32 Frame = In.Frame;

	// One frame after the window: the freeze of the hitstop finishes on the first frame outside it, and Mover hands the finish velocity of a
	// finishing layered move back over whatever the tick proposes, so a move started on that frame would be overwritten (found in PIE). The
	// move also keeps its preferred mode only on its own first tick
	if (Core.Hit.HasPendingReaction() && Frame > Core.HitstopEndFrame)
	{
		const FVector Launch = Core.Hit.GetPendingLaunch();
		const float Pass = 1.f - FMath::Clamp(In.KnockbackResistance, 0.f, 1.f);
		const int32 Frames = FMath::Max(Core.Hit.HitstunEndFrame - Frame, 1);
		switch (Core.Hit.GetPendingReaction())
		{
		case EGSTN_HitReaction::Launch:
		{
			const FGSTN_DIParams DIParams = In.Rules ? In.Rules->DI : FGSTN_DIParams();
			const FVector Plane = StickToPlane(In.Stick, In.CameraYawDegrees, In.Up);
			const FVector Fallback = UGSTN_HitMathUtils::ProjectOnPlane(In.Facing, In.Up).GetSafeNormal();
			Move.Kind = FGSTN_ReactionMove::EKind::Launch;
			Move.Velocity = UGSTN_HitMathUtils::ApplyDI(Launch, Plane, In.Up, Fallback, DIParams) * Pass;
			Move.GravityScale = 1.f;
			Move.Frames = Frames;
			break;
		}
		case EGSTN_HitReaction::PushBack:
			Move.Kind = FGSTN_ReactionMove::EKind::PushBack;
			Move.Velocity = Launch * Pass;
			Move.DecayPerFrame = 1.f / static_cast<float>(Frames);
			Move.Frames = Frames;
			break;
		default:
			// StaggerInPlace: the freeze ended at rest, nothing moves
			break;
		}
		Core.Hit.ClearPendingReaction();
	}

	if (!Core.Hit.HasPendingReaction() && Frame >= Core.Hit.HitstunEndFrame && Frame >= Core.HitstopEndFrame)
	{
		// Hitstun is over. A tumble goes on until the victim stands on something
		if (Core.ActionState == EGSTN_ActionState::Hitstun || (Core.ActionState == EGSTN_ActionState::Tumble && In.bGrounded))
		{
			Core.ActionState = EGSTN_ActionState::Actionable;
		}
	}
	return Move;
}
