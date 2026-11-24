// Combat simulation (GAS-NPP fork): the part of the combat component that runs the victim side of the tick (combat-m4-hits 7, M4 H9) and
// executes what it decided in the ability system's pre-tick.

#include "Simulation/GSTN_CombatSimComponent.h"
#include "Timeline/GSTN_TimelineTypes.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Context/GSTN_InputContextProvider.h"
#include "Debug/GSTN_CombatDebugDraw.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/GSTN_HealthAttributeSet.h"
#include "Events/GSTN_HitEvents.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "GSTN_HitEffectContext.h"
#include "GameFramework/Actor.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputTags.h"
#include "GSTN_SurfaceProvider.h"
#include "GSTN_SurfaceUtils.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Movement/GSTN_GravityFrame.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverTypes.h"
#include "Rules/GSTN_AirCrouch.h"
#include "Rules/GSTN_GroundCrouch.h"
#include "Movement/GSTN_BodyCapsuleModifier.h"
#include "Rules/GSTN_CombatRules.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "World/GSTN_CombatWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatVictim, Log, All);

void UGSTN_CombatSimComponent::RunVictimStep(const FNetSimTimeStep& TimeStep, const FGSTN_CombatSyncState& InSync, FGSTN_CombatSyncState& Sync, const FGSTN_StepSnapshot* Snapshot, const FGSTN_InputCmd& Cmd)
{
	const int32 Frame = TimeStep.Frame;
	FGSTN_CombatSyncCore& Core = Sync.Core;
	VictimFrame = Frame;
	bFrameInterrupted = false;
	ReactionMove = FGSTN_ReactionMove();

	// Messages written for this frame by the senders of the previous one, in (sender, kind, hitbox, hurtbox) order. The entries of the other kinds
	// go to their handlers now, before the frame's hits are resolved; the hits are collected for the victim step as one batch
	InboxScratch.Reset();
	InboxEntryScratch.Reset();
	DrainInbox(Frame, InboxEntryScratch);
	FGSTN_InboxDispatchContext InboxContext;
	InboxContext.Frame = Frame;
	InboxContext.Sync = &Sync;
	InboxContext.Receiver = this;
	for (const FGSTN_InboxEntry& InboxEntry : InboxEntryScratch)
	{
		if (const FGSTN_HitInboxEntry* HitEntry = FGSTN_HitInboxEntry::FromInboxEntry(InboxEntry))
		{
			InboxScratch.Add(*HitEntry);
		}
		else
		{
			FGSTN_InboxKindRegistry::Get().Dispatch(InboxEntry, InboxContext);
		}
	}
	if (GSTNCombatDebug::IsHitLogOn())
	{
		InboxLogByFrame.Remove(Frame);
		FString Log;
		for (const FGSTN_HitInboxEntry& Entry : InboxScratch)
		{
			Log += FString::Printf(TEXT("%d:%d:%d:%d:%d:%d:%d:%d;"), Frame, Entry.AttackerId, Entry.AttackerHitboxIndex, Entry.HurtboxIndex, Entry.HurtState,
				Entry.bRicochet ? 1 : 0, Entry.Relationship, FMath::RoundToInt(Entry.Hit.Damage));
		}
		if (!Log.IsEmpty())
		{
			InboxLogByFrame.Add(Frame, Log);
			InboxLogByFrame.Remove(Frame - 512);
		}
	}

	// The Mover context at the end of the previous frame, and the victim's own pose
	FGameplayTagContainer ContextTags;
	if (ContextProvider)
	{
		ContextProvider->AppendContextTags(ContextTags);
	}
	const bool bGrounded = ContextTags.HasTagExact(GSTNInputTags::Context_Grounded);
	const bool bOnWall = ContextTags.HasTagExact(GSTNInputTags::Context_OnWall);

	const FGSTN_SnapshotEntry* Self = Snapshot && RegisteredEntityId != INDEX_NONE ? Snapshot->Find(RegisteredEntityId) : nullptr;

	FGSTN_VictimTickInput In;
	In.Frame = Frame;
	In.Self = Self;
	In.Rules = CombatRules;
	In.Entries = InboxScratch;
	In.bGrounded = bGrounded;
	In.bOnWall = bOnWall;
	In.Fragments = &Sync.Fragments;
	In.Snapshot = Snapshot;
	FGSTN_CombatVictim::Resolve(In, Core, VictimResult);
	// The reverse deliveries of the hits that stood. A target that does not tick on this machine (an interpolated proxy of the attacker) refuses
	// the entry, so on a client they only reach an attacker it predicts, as the hits themselves do
	for (const FGSTN_VictimOutgoing& Reply : VictimResult.Outgoing)
	{
		PostToInbox(Reply.TargetId, Reply.Entry);
	}

	// A KO zone: the position at the end of the previous frame against the static zones of the world. A hit that knocked it out this frame
	// stands (the reason is Health), the zone only knocks out what is not out yet
	if (Self && VictimResult.KO == EGSTN_KOCause::None)
	{
		const UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
		bool bCancelledMove = false;
		if (World && FGSTN_CombatVictim::ApplyZoneKO(Core, Frame, World->IsInKOZone(Self->Transform.GetLocation()), bCancelledMove))
		{
			VictimResult.KO = EGSTN_KOCause::Zone;
			VictimResult.bCancelledMove |= bCancelledMove;
		}
	}
	// The tick extensions' view of the victim step (M6 F8: the landing and the tech read the contact, the DI rules the launch). No services: the
	// frame's tables are chosen after this step
	const FGSTN_CombatTickExtensionRegistry& Extensions = FGSTN_CombatTickExtensionRegistry::Get();
	FGSTN_CombatTickContext VictimContext(Frame, InSync, Sync);
	{
		FVector Up = FVector::UpVector;
		if (Self)
		{
			Up = Self->Up;
		}
		else
		{
			FTransform Pose;
			ReadOwnPose(Pose, Up);
		}
		FillHookContext(VictimContext, Cmd, Up);
		if (Self)
		{
			VictimContext.Velocity = Self->Velocity;
		}
	}

	// The surface impact of a tumbling victim: the contact the knockback move's probe made on the frame before, from Mover's tracked state
	ImpactResult = FGSTN_ImpactStepResult();
	if (Self && (ImpactTable || CombatRules || !Extensions.IsEmpty()) && Core.ActionState == EGSTN_ActionState::Tumble)
	{
		FGSTN_ImpactStepInput ImpactIn;
		ImpactIn.Frame = Frame;
		ImpactIn.Rules = CombatRules;
		ImpactIn.Table = ImpactTable;
		ImpactIn.Response = KitImpactResponse.IsSet() ? &KitImpactResponse.GetValue() : (CombatRules ? &CombatRules->ImpactResponse : nullptr);
		ImpactIn.Contact = ReadTumbleContact();
		ImpactIn.Velocity = Self->Velocity;
		++DebugTumbleFrames;
		if (ImpactIn.Contact.IsSet())
		{
			++DebugContactFrames;
			const FString Seen = FString::Printf(TEXT("c%d:k%d:s%d:t%d"), ImpactIn.Contact.ContactFrame, static_cast<int32>(ImpactIn.Contact.Kind), ImpactIn.Contact.ImpactSpeedQ, ImpactIn.Contact.SurfaceTypeIndex);
			if (!DebugLastContact.EndsWith(Seen))
			{
				// Every distinct record the decision saw, with the frame it was first read on
				DebugLastContact = (DebugLastContact == TEXT("none") ? FString() : DebugLastContact.Right(900) + TEXT("|")) + FString::Printf(TEXT("r%d>"), Frame) + Seen;
			}
		}
		for (const FGSTN_VictimHit& Hit : VictimResult.Hits)
		{
			ImpactIn.bStaggeredThisFrame |= Hit.bStaggered;
		}
		// A contact the impact step would decide this frame (the same conditions): the extensions see it before (a tech consumes it) and after
		const FGSTN_TumbleContactSyncState& Contact = ImpactIn.Contact;
		const bool bNewContact = Contact.IsSet() && Contact.ContactFrame == Frame - 1 && !ImpactIn.bStaggeredThisFrame && !Core.IsInHitstop(Frame)
			&& !(Core.Hit.ImpactEndFrame != INDEX_NONE && Contact.ContactFrame < Core.Hit.ImpactEndFrame);
		const bool bConsumed = bNewContact && Extensions.ConsumeTumbleContact(VictimContext, Contact);
		// Without a surface table only the rules' defaults can resolve: the ground bounce, the wall and ceiling bounce (StepImpact checks the face)
		if (!bConsumed)
		{
			FGSTN_CombatVictim::StepImpact(ImpactIn, Core, ImpactResult);
		}
		if (bNewContact && !bConsumed)
		{
			Extensions.OnTumbleContactResolved(VictimContext, Contact, ImpactResult.Outcome);
		}
		if (ImpactResult.Outcome != EGSTN_ImpactOutcome::None)
		{
			AppendImpactEvent(Frame);
			if (ImpactResult.Outcome == EGSTN_ImpactOutcome::BreakSurface)
			{
				if (const TWeakObjectPtr<UPrimitiveComponent>* Surface = ContactSurfaces.Find(ImpactIn.Contact.ContactFrame))
				{
					BreakRequests.Add(Frame, *Surface);
					DebugBreaks = (DebugBreaks + FString::Printf(TEXT("req%d@sim%d "), Frame, GetSimulationFrame())).Right(400);
				}
			}
			if (VictimResult.FreezeFrames == 0 && ImpactResult.FreezeFrames > 0)
			{
				VictimResult.FreezeFrames = ImpactResult.FreezeFrames;
				VictimResult.bFreezeZeroVelocity = ImpactResult.bFreezeAtRest;
			}
			LastImpactInfo = FString::Printf(TEXT("%d,%d,%d,%d,%d"), Frame, static_cast<int32>(ImpactResult.Outcome), static_cast<int32>(ImpactResult.SurfaceTypeIndex),
				FMath::RoundToInt(ImpactResult.Resolved.JudgedSpeed), FMath::RoundToInt(ImpactResult.Damage));
			++ImpactCounts[FMath::Clamp(static_cast<int32>(ImpactResult.Outcome) - 1, 0, 2)];
		}
	}
	AppendVictimEvents(Frame);

	if (!InboxScratch.IsEmpty() && !Self)
	{
		UE_LOG(LogGSTNCombatVictim, Warning, TEXT("%s frame %d: %d hits drained but the entity has no snapshot entry (id %d): they are dropped"), *GetPathName(), Frame, InboxScratch.Num(), RegisteredEntityId);
	}

	// The requests the pre-tick of this frame executes. One of a kind covers every hit of the frame: the executor walks VictimResult
	const FGSTN_VictimResult& Result = VictimResult;
	bool bAnyEffects = false;
	bool bAnyCue = false;
	for (const FGSTN_VictimHit& Hit : Result.Hits)
	{
		bFrameInterrupted |= !Hit.bRicochet;
		bAnyEffects |= !Hit.bRicochet && !Hit.Outcome.Effects.IsEmpty();
		bAnyCue |= Hit.Entry.Hit.CueTag.IsValid();
	}
	if (!Result.Hits.IsEmpty())
	{
		Core.Requests.Add(EGSTN_RequestType::ApplyDamage, Frame, static_cast<int16>(FMath::Min(Result.Hits.Num(), 32767)));
	}
	if (bAnyEffects)
	{
		Core.Requests.Add(EGSTN_RequestType::ApplyEffect, Frame, static_cast<int16>(FMath::Min(Result.Hits.Num(), 32767)));
	}
	if (bAnyCue)
	{
		Core.Requests.Add(EGSTN_RequestType::Cue, Frame, static_cast<int16>(FMath::Min(Result.Hits.Num(), 32767)));
	}
	if (Result.FreezeFrames > 0)
	{
		Core.Requests.Add(EGSTN_RequestType::VictimFreeze, Frame, static_cast<int16>(FMath::Min(Result.FreezeFrames, 32767)));
	}

	// The turn of a staggered victim, on the hit frame (react-hook-4 review: turned at the launch, the first 7 hitstop frames showed the
	// reaction pose the wrong way round). Queued like a steer: the pre-tick of this frame hands it to Mover before the hitstop freeze holds it
	if (bFrameInterrupted)
	{
		float TurnYaw = 0.f;
		const bool bTurn = Self && FGSTN_CombatVictim::ResolveHitFacing(CombatRules, Result, Core, *Self, Snapshot, TurnYaw);
		bool bQueued = false;
		if (bTurn)
		{
			const int16 YawArg = FGSTN_StartupSteer::QuantizeYaw(TurnYaw);
			if (YawArg != FGSTN_StartupSteer::QuantizeYaw(FGSTN_GravityFrame::YawOfDirection(Self->Facing, Self->Up)))
			{
				bQueued = Core.Requests.Add(EGSTN_RequestType::SetFacing, Frame, YawArg);
				if (!bQueued)
				{
					UE_LOG(LogGSTNCombatVictim, Error, TEXT("%s frame %d: no room for the hit turn request (%d requests)"), *GetOwner()->GetName(), Frame, Core.Requests.Num());
				}
			}
		}
		// Diagnostics of the react-hook-5 capture (the turn did not show in game): one line per interrupting hit
		UE_LOG(LogGSTNCombatVictim, Verbose, TEXT("[HitTurn] %s frame %d self=%d rules=%d pending=%d turn=%d yaw=%.0f selfYaw=%.0f queued=%d requests=%d"),
			*GetOwner()->GetName(), Frame, Self ? 1 : 0, CombatRules ? 1 : 0, Core.Hit.HasPendingReaction() ? 1 : 0, bTurn ? 1 : 0, TurnYaw,
			Self ? Self->Facing.Rotation().Yaw : 0.0, bQueued ? 1 : 0, Core.Requests.Num());
	}

	// The end of the hitstop and of the hitstun, by the reaction the hit chose
	FGSTN_ReactionStepInput Reaction;
	Reaction.Frame = Frame;
	Reaction.Rules = CombatRules;
	Reaction.bGrounded = bGrounded;
	if (Self)
	{
		Reaction.Up = Self->Up;
		Reaction.Facing = Self->Facing;
	}
	else
	{
		FTransform Pose;
		ReadOwnPose(Pose, Reaction.Up);
		Reaction.Facing = Pose.GetUnitAxis(EAxis::X);
	}
	Reaction.Stick = FVector2D(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickY));
	Reaction.CameraYawDegrees = FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw);
	Reaction.bTumbleAirRecovery = IsTumbleAirRecoveryOn();
	if (AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UGSTN_HealthAttributeSet::GetKnockbackResistanceAttribute()))
	{
		Reaction.KnockbackResistance = AbilitySystem->GetNumericAttribute(UGSTN_HealthAttributeSet::GetKnockbackResistanceAttribute());
	}
	if (!Extensions.IsEmpty())
	{
		Reaction.DIOverride = [&Extensions, &VictimContext](const FVector& Launch, const FVector& Plane, const FVector& Up, const FVector& Fallback, FVector& Out)
		{
			return Extensions.OverrideDI(VictimContext, Launch, Plane, Up, Fallback, Out);
		};
	}
	ReactionMove = FGSTN_CombatVictim::StepReaction(Reaction, Core);
	if (ReactionMove.Kind == FGSTN_ReactionMove::EKind::None && ImpactResult.Move.Kind != FGSTN_ReactionMove::EKind::None)
	{
		// A bounce: the relaunch of the victim off the surface goes out like the launch of a hit
		ReactionMove = ImpactResult.Move;
	}
	if (ImpactResult.Damage > 0.f)
	{
		Core.Requests.Add(EGSTN_RequestType::ImpactDamage, Frame, static_cast<int16>(FMath::Clamp(FMath::RoundToInt(ImpactResult.Damage), 1, 32767)));
	}
	if (ReactionMove.Kind != FGSTN_ReactionMove::EKind::None)
	{
		Core.Requests.Add(ReactionMove.Kind == FGSTN_ReactionMove::EKind::Launch ? EGSTN_RequestType::Knockback : EGSTN_RequestType::PushBack, Frame, 0);
	}
}

FGSTN_TumbleContactSyncState UGSTN_CombatSimComponent::ReadTumbleContact()
{
	FGSTN_TumbleContactSyncState Latest;
	const UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
	if (!Tracker)
	{
		return Latest;
	}
	// Every knockback move alive in the tracked state carries a record (a bounce starts a second one beside the first): the newest wins
	for (const TSharedPtr<FLayeredMoveBase>& Move : Tracker->GetSyncState().LayeredMoves.GetActiveMoves())
	{
		if (Move.IsValid() && Move->GetScriptStruct() == FGSTN_LayeredMove_Knockback::StaticStruct())
		{
			const FGSTN_TumbleContactSyncState& Contact = static_cast<const FGSTN_LayeredMove_Knockback&>(*Move).Contact;
			if (Contact.IsSet() && (!Latest.IsSet() || Contact.ContactFrame > Latest.ContactFrame))
			{
				Latest = Contact;
			}
		}
	}
	return Latest;
}

void UGSTN_CombatSimComponent::NoteContactSurface(int32 Frame, UPrimitiveComponent* Component)
{
	++DebugNotes;
	ContactSurfaces.Add(Frame, Component);
	ContactSurfaces.Remove(Frame - 64);
}

FString UGSTN_CombatSimComponent::GetImpactInfo() const
{
	return LastImpactInfo;
}

FString UGSTN_CombatSimComponent::GetImpactDebug() const
{
	const FGSTN_LayeredMove_Knockback::FProbeStats& P = FGSTN_LayeredMove_Knockback::ProbeStats;
	return FString::Printf(TEXT("%d,%d,%d,%d,%s,breaks %s,probe calls %d nosweep %d nohit %d pen %d unset %d found %d last %s"), ImpactTable ? 1 : 0, DebugNotes, DebugTumbleFrames, DebugContactFrames, *DebugLastContact, *DebugBreaks,
		P.Calls, P.NoSweep, P.NoHit, P.Penetrating, P.Unset, P.Found, *P.Last);
}

FString UGSTN_CombatSimComponent::GetImpactCounts() const
{
	return FString::Printf(TEXT("%d,%d,%d"), ImpactCounts[0], ImpactCounts[1], ImpactCounts[2]);
}

void UGSTN_CombatSimComponent::AppendImpactEvent(int32 Frame)
{
	FGSTN_SimEvent Event;
	switch (ImpactResult.Outcome)
	{
	case EGSTN_ImpactOutcome::Splat: Event.EventType = GSTNCombatTags::Event_Impact_Splat; break;
	case EGSTN_ImpactOutcome::Bounce: Event.EventType = ImpactResult.bBump ? GSTNCombatTags::Event_Impact_Bump : GSTNCombatTags::Event_Impact_Bounce; break;
	case EGSTN_ImpactOutcome::BreakSurface: Event.EventType = GSTNCombatTags::Event_Impact_Break; break;
	default: return;
	}
	Event.Frame = Frame;
	Event.Instigator = GetOwner();
	Event.Target = GetOwner();
	Event.Magnitude = ImpactResult.Resolved.JudgedSpeed;
	// The surface type names the surface to the presentation (a thud, a shatter); the tag is made from the index here, outside any state
	const FGameplayTag SurfaceType = UGSTN_SurfaceUtils::GetSurfaceTypeFromIndex(ImpactResult.SurfaceTypeIndex);
	if (SurfaceType.IsValid())
	{
		Event.Tags.AddTag(SurfaceType);
	}
	EventOutbox.Add(MoveTemp(Event));
}

void UGSTN_CombatSimComponent::ExecuteImpactDamage(float Damage)
{
	UClass* Class = CombatRules && !CombatRules->DamageEffect.IsNull() ? CombatRules->DamageEffect.LoadSynchronous() : nullptr;
	if (!AbilitySystem || !Class || !Class->IsChildOf(UGameplayEffect::StaticClass()) || Damage <= 0.f)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Class, 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_Damage, Damage);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UGSTN_CombatSimComponent::ExecuteSurfaceBreaks()
{
	const bool bAuthority = GetOwner() && GetOwner()->HasAuthority();
	for (const TPair<int32, TWeakObjectPtr<UPrimitiveComponent>>& Request : BreakRequests)
	{
		UPrimitiveComponent* Collider = bAuthority ? Request.Value.Get() : nullptr;
		if (!Collider)
		{
			continue;
		}
		DebugBreaks = (DebugBreaks + FString::Printf(TEXT("exec%d@sim%d "), Request.Key, GetSimulationFrame())).Right(400);
		// The collider answers for itself, else its actor (the same order the surface resolution uses)
		if (Collider->Implements<UGSTN_SurfaceProvider>())
		{
			IGSTN_SurfaceProvider::Execute_BreakSurface(Collider);
		}
		else if (AActor* Owner = Collider->GetOwner(); Owner && Owner->Implements<UGSTN_SurfaceProvider>())
		{
			IGSTN_SurfaceProvider::Execute_BreakSurface(Owner);
		}
	}
	BreakRequests.Reset();
}

void UGSTN_CombatSimComponent::AppendVictimEvents(int32 Frame)
{
	for (const FGSTN_VictimHit& Hit : VictimResult.Hits)
	{
		AActor* Attacker = FindAttackerActor(Hit);
		FGameplayTagContainer MoveTags;
		if (const UGSTN_CombatSimComponent* AttackerCombat = Attacker ? Attacker->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr)
		{
			AttackerCombat->AppendMoveTags(Hit.Entry.AttackerTimeline, Hit.Entry.AttackerBinding, MoveTags);
		}
		const int32 EventsBefore = EventOutbox.Events.Num();
		FGSTN_CombatVictim::AppendHitEvents(Hit, Frame, Attacker, GetOwner(), MoveTags, EventOutbox, FindCreditPartnerActor(Hit));

		// The typed copy for the Blueprint events (On Hit Received / Landed), with the tags of the HitLanded event just written. Local data like
		// the outbox, written on every machine without a branch; only the authority's is ever sent (SendConfirmedEvents)
		const FGameplayTagContainer* EventTags = &MoveTags;
		for (int32 Index = FMath::Min(EventsBefore, EventOutbox.Events.Num()); Index < EventOutbox.Events.Num(); ++Index)
		{
			if (EventOutbox.Events[Index].EventType == GSTNCombatTags::Event_HitLanded)
			{
				EventTags = &EventOutbox.Events[Index].Tags;
				break;
			}
		}
		if (HitEventOutbox.Num() >= FGSTN_SimEventOutbox::MaxEvents)
		{
			HitEventOutbox.RemoveAt(0);
		}
		HitEventOutbox.Add(FGSTN_CombatHitEventData::FromVictimHit(Hit, Frame, FGSTN_CombatVictim::CreditedAttacker(Attacker, Hit.Entry), GetOwner(), *EventTags));
	}
	if (VictimResult.KO != EGSTN_KOCause::None)
	{
		const FGSTN_VictimHit* KOHit = VictimResult.Hits.IsValidIndex(VictimResult.KOHitIndex) ? &VictimResult.Hits[VictimResult.KOHitIndex] : nullptr;
		AActor* Attacker = KOHit ? FindAttackerActor(*KOHit) : nullptr;
		FGSTN_CombatVictim::AppendKOEvent(VictimResult.KO, Frame, Attacker, GetOwner(), EventOutbox, KOHit ? &KOHit->Outcome.Credit : nullptr, KOHit ? FindCreditPartnerActor(*KOHit) : nullptr,
			KOHit ? &KOHit->Entry : nullptr);
	}
}

void UGSTN_CombatSimComponent::AppendMoveTags(int32 Timeline, int32 Binding, FGameplayTagContainer& OutTags) const
{
	const UGSTN_CombatTableSet& Set = Tables();
	if (Set.Table.Bindings.IsValidIndex(Binding))
	{
		const int32 CommandIndex = Set.Table.Bindings[Binding].CommandIndex;
		if (Set.Table.Commands.IsValidIndex(CommandIndex))
		{
			OutTags.AddTag(Set.Table.Commands[CommandIndex].Tag);
		}
	}
	if (Set.TimelineRegistry.IsValidIndex(Timeline) && Set.TimelineRegistry[Timeline])
	{
		OutTags.AppendTags(Set.TimelineRegistry[Timeline]->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetAssetTags());
	}
}

bool UGSTN_CombatSimComponent::ServerRespawn()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	return NetworkPredictionProxy.WriteSyncState<FGSTN_CombatSyncState>([](FGSTN_CombatSyncState& Sync)
	{
		if (Sync.Core.ActionState == EGSTN_ActionState::KO)
		{
			Sync.Core.ActionState = EGSTN_ActionState::Actionable;
		}
	}, "Respawn") != nullptr;
}

FString UGSTN_CombatSimComponent::GetVictimInfo() const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (!Sync)
	{
		return TEXT("-1,0,-1,-1,-1,0");
	}
	return FString::Printf(TEXT("%d,%d,%d,%d,%d,%d"), Sync->Core.Input.History.GetLatestFrame(), static_cast<int32>(Sync->Core.ActionState), Sync->Core.HitstopStartFrame,
		Sync->Core.HitstopEndFrame, Sync->Core.Hit.HitstunEndFrame, Sync->Core.Hit.HasPendingReaction() ? static_cast<int32>(Sync->Core.Hit.GetPendingReaction()) + 1 : 0);
}

FString UGSTN_CombatSimComponent::GetRecoilInfo() const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (!Sync)
	{
		return TEXT("-1,0,-1,-1,-1");
	}
	return FString::Printf(TEXT("%d,%d,%d,%d,%d"), Sync->Core.Input.History.GetLatestFrame(), static_cast<int32>(Sync->Core.ActionState), static_cast<int32>(Sync->Core.ActiveTimeline),
		Sync->Core.TimelineStartFrame, Sync->Core.RecoilStartFrame);
}

void UGSTN_CombatSimComponent::ProjectStateTags(const FGSTN_CombatSyncCore& Core, int32 Frame)
{
	if (AbilitySystem)
	{
		FGSTN_CombatVictim::ProjectStateTags(*AbilitySystem, Core, Frame);
		// Set from the state like the reaction tags: the hook a dodge landing clip attaches to (owner 2026-11-17)
		AbilitySystem->SetLooseGameplayTagCount(GSTNCombatTags::State_Dodge_EndLag, Core.DodgeLanding.IsEndLag(Frame) ? 1 : 0);
		ProjectAirCrouchTag(Core);
		ProjectGroundCrouch(Core);
	}
}

void UGSTN_CombatSimComponent::ProjectGroundCrouch(const FGSTN_CombatSyncCore& Core)
{
	// Same sources as the air crouch: the kit of the revision the state runs, Mover's ground and the previous frame's crouch from the tracker
	// (rolled back with Mover, still the end of the previous frame here), the frame's held buttons. A function of those only, so a resimulated
	// frame decides the same
	const int32 Frame = Core.Input.History.GetLatestFrame();
	const UGSTN_CombatTableSet* Set = bNativeGroundCrouch ? FindTableSet(Core.TableRevision) : nullptr;
	bool bStance = false;
	bool bTag = false;
	float HalfHeight = 0.f;
	float Radius = 0.f;
	if (Set && Set->KitGroundCrouch.bEnabled)
	{
		const FGSTN_GroundCrouchKitExtension& Crouch = Set->KitGroundCrouch;
		const int32 Button = ButtonMap.GetIndex(Crouch.ResolveHoldButton());
		const bool bHeld = Button >= 0 && Button < 32 && (Core.Input.History.GetHeldButtons() & (1u << Button)) != 0;
		const UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
		const bool bGrounded = bHeld && Tracker && Tracker->HasGameplayTag(Mover_IsOnGround, true);
		if (bGrounded)
		{
			const bool bWasCrouched = FGSTN_BodyCapsuleModifier::IsCrouchedIn(Tracker->GetSyncState());
			FGameplayTagContainer OwnerTags;
			if (!Crouch.BlockedTags.IsEmpty())
			{
				AbilitySystem->GetOwnedGameplayTags(OwnerTags);
			}
			const bool bInMove = Core.ActionState == EGSTN_ActionState::InTimeline;
			const bool bLocked = FGSTN_CombatVictim::IsActionLocked(Core);
			bStance = Crouch.HoldsStance(true, true, bInMove, bLocked, bWasCrouched, OwnerTags);
			bTag = Crouch.HoldsTag(true, true, bInMove, bLocked, bWasCrouched, OwnerTags);
		}
		HalfHeight = Crouch.CapsuleHalfHeight;
		Radius = Crouch.CapsuleRadius;
	}
	FrameCrouch.Frame = Frame;
	FrameCrouch.bCrouch = bStance;
	FrameCrouch.HalfHeight = HalfHeight;
	FrameCrouch.Radius = Radius;
	AbilitySystem->SetLooseGameplayTagCount(GSTNCombatTags::State_Crouch, bTag ? 1 : 0);
}

bool UGSTN_CombatSimComponent::ReadFrameGroundCrouch(bool& bOutCrouch, float& OutHalfHeight, float& OutRadius) const
{
	if (FrameCrouch.Frame == INDEX_NONE || FrameCrouch.Frame != GetSimulationFrame())
	{
		return false;
	}
	bOutCrouch = FrameCrouch.bCrouch;
	OutHalfHeight = FrameCrouch.HalfHeight;
	OutRadius = FrameCrouch.Radius;
	return true;
}

void UGSTN_CombatSimComponent::ProjectAirCrouchTag(const FGSTN_CombatSyncCore& Core)
{
	// Set, not added, like the reaction states: a function of this frame's state only. The kit's data comes from the table of the revision the
	// state runs (a rollback across a kit swap reads the kit of that frame); the ground from the Mover tracker, which is rolled back with Mover and
	// still holds the end of the previous frame here (the ability system ticks before Mover); the button from this frame's held buttons
	const UGSTN_CombatTableSet* Set = FindTableSet(Core.TableRevision);
	bool bAirCrouch = false;
	if (Set && Set->bHasKitAirCrouch)
	{
		const FGSTN_AirCrouchKitExtension& AirCrouch = Set->KitAirCrouch;
		const int32 Button = ButtonMap.GetIndex(AirCrouch.ResolveHoldButton());
		const bool bHeld = Button >= 0 && Button < 32 && (Core.Input.History.GetHeldButtons() & (1u << Button)) != 0;
		const UGSTN_MoverSimStateTracker* Tracker = bHeld ? GetJumpTracker() : nullptr;
		// In air means Mover's air modes (falling, flying): a wall mode is neither ground nor air, so a wall slide never air-crouches (crouch
		// detaches from the wall there, and the air crouch starts on the next frame in the air)
		const bool bAirborne = Tracker && Tracker->HasGameplayTag(Mover_IsInAir, true);
		if (bAirborne)
		{
			FGameplayTagContainer OwnerTags;
			if (!AirCrouch.BlockedTags.IsEmpty())
			{
				AbilitySystem->GetOwnedGameplayTags(OwnerTags);
			}
			bAirCrouch = AirCrouch.Holds(true, true, Core.ActionState == EGSTN_ActionState::InTimeline, FGSTN_CombatVictim::IsActionLocked(Core), OwnerTags);
		}
	}
	AbilitySystem->SetLooseGameplayTagCount(GSTNCombatTags::State_AirCrouch, bAirCrouch ? 1 : 0);
}

AActor* UGSTN_CombatSimComponent::FindAttackerActor(const FGSTN_VictimHit& Hit) const
{
	const UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	return World ? World->FindOwnerActor(Hit.Entry.AttackerId) : nullptr;
}

AActor* UGSTN_CombatSimComponent::FindCreditPartnerActor(const FGSTN_VictimHit& Hit) const
{
	const UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	return World && Hit.Outcome.Credit.IsSet() ? World->FindOwnerActor(Hit.Outcome.Credit.PartnerEntityId) : nullptr;
}

FGameplayEffectContextHandle UGSTN_CombatSimComponent::MakeHitContext(const FGSTN_VictimHit& Hit) const
{
	AActor* Attacker = FindAttackerActor(Hit);
	FGameplayEffectContextHandle Handle = FGSTN_HitEffectContext::Make(Attacker ? Attacker : GetOwner(), Attacker ? Attacker : GetOwner(), CombatRules);
	if (FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandleMutable(Handle))
	{
		Context->HitLocation = FVector_NetQuantize10(Hit.Entry.HitLocation);
		Context->Relationship = GSTNHitRelationship::FromIndex(Hit.Entry.Relationship);
		Context->Reaction = Hit.Reaction;
		Context->bStaggered = Hit.bStaggered;
		Context->HurtboxIndex = Hit.Entry.HurtboxIndex;
		// With the attacker and the victim these identify the hit for the attacker's own presentation (UGSTN_HitPresentationComponent::MakeHitId)
		Context->TimelineIndex = Hit.Entry.AttackerTimeline;
		Context->HitboxIndex = Hit.Entry.AttackerHitboxIndex;
	}
	return Handle;
}

bool UGSTN_CombatSimComponent::ApplyHitEffect(const TSoftClassPtr<UObject>& Effect, const FGSTN_VictimHit& Hit, float DamageMagnitude)
{
	UClass* Class = Effect.IsNull() ? nullptr : Effect.LoadSynchronous();
	if (!AbilitySystem || !Class || !Class->IsChildOf(UGameplayEffect::StaticClass()))
	{
		return false;
	}
	// The spec is made by the attacker's ability system when it is found, so the damage execution captures the attacker's multipliers; it is applied to this entity
	UAbilitySystemComponent* Source = AbilitySystem;
	if (const AActor* Attacker = FindAttackerActor(Hit))
	{
		if (UAbilitySystemComponent* AttackerAsc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker))
		{
			Source = AttackerAsc;
		}
	}
	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Class, 1.f, MakeHitContext(Hit));
	if (!Spec.IsValid())
	{
		return false;
	}
	if (DamageMagnitude != 0.f)
	{
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_Damage, DamageMagnitude);
	}
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}

void UGSTN_CombatSimComponent::ExecuteVictimHits(int32 Frame)
{
	if (VictimFrame != Frame || !AbilitySystem)
	{
		return;
	}
	for (const FGSTN_VictimHit& Hit : VictimResult.Hits)
	{
		if (!Hit.bRicochet)
		{
			for (const FGSTN_HitRequest& Request : Hit.Outcome.Requests)
			{
				if (Request.Kind == EGSTN_HitRequestKind::Damage && Request.Target == EGSTN_HitRequestTarget::Victim)
				{
					ApplyHitEffect(Request.Effect, Hit, Request.Magnitude);
				}
			}
		}

		// The gameplay event of the hit, to the victim's abilities (a counter, a hurt reaction). The payload carries the reaction
		// and whether it staggered; reaction abilities may listen, they never set combat state
		FGSTN_HitEventPayload Payload;
		Payload.Reaction = Hit.Reaction;
		Payload.bStaggered = Hit.bStaggered;
		Payload.HurtboxIndex = Hit.Entry.HurtboxIndex;
		Payload.Relationship = GSTNHitRelationship::FromIndex(Hit.Entry.Relationship);
		FGameplayEventData EventData = Payload.MakeEventData(FindAttackerActor(Hit), GetOwner(), CombatRules);
		// The hitstun of the hit, for presentation listeners (the directional hit additive scales its weight with it); nothing in the sim reads it
		EventData.EventMagnitude = static_cast<float>(Hit.HitstunFrames);
		AbilitySystem->HandleGameplayEvent(GSTNCombatTags::Event_Hit_Received, &EventData);
	}
}

void UGSTN_CombatSimComponent::ExecuteVictimEffects(int32 Frame)
{
	if (VictimFrame != Frame)
	{
		return;
	}
	for (const FGSTN_VictimHit& Hit : VictimResult.Hits)
	{
		if (Hit.bRicochet)
		{
			continue;
		}
		for (const TSoftClassPtr<UObject>& Effect : Hit.Outcome.Effects)
		{
			ApplyHitEffect(Effect, Hit, 0.f);
		}
	}
}

void UGSTN_CombatSimComponent::ExecuteVictimCues(int32 Frame)
{
	if (VictimFrame != Frame || !AbilitySystem)
	{
		return;
	}
	for (const FGSTN_VictimHit& Hit : VictimResult.Hits)
	{
		if (Hit.Entry.Hit.CueTag.IsValid())
		{
			AbilitySystem->ExecuteGameplayCue(Hit.Entry.Hit.CueTag, MakeHitContext(Hit));
		}
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogGSTNReactionMove, Log, All);

void UGSTN_CombatSimComponent::ExecuteReactionMove(float StepMs)
{
	if (!AbilitySystem || ReactionMove.Kind == FGSTN_ReactionMove::EKind::None)
	{
		return;
	}
	const bool bLaunch = ReactionMove.Kind == FGSTN_ReactionMove::EKind::Launch;
	// Back first along the knockback (rules data, owner 2026-11-14): the facing is Mover sync state, queued as the steer's instant effect on the
	// same pre-tick as the move, so a resimulated frame turns the same way. A vertical launch keeps the facing
	// On the gravity floor, yaw about the local up (gravity audit 2026-11-16)
	FTransform Pose;
	FVector Up = FVector::UpVector;
	ReadOwnPose(Pose, Up);
	const FVector Flat = FGSTN_GravityFrame::Flatten(ReactionMove.Velocity, Up);
	if (CombatRules && CombatRules->bTurnBackAlongKnockback && Flat.SizeSquared() > 1.0)
	{
		ExecuteSetFacing(FGSTN_StartupSteer::QuantizeYaw(FGSTN_GravityFrame::YawOfDirection(-Flat, Up)));
	}
	const TSubclassOf<UGameplayEffect> Effect = bLaunch ? KnockbackEffect : PushBackEffect;
	UE_LOG(LogGSTNReactionMove, Verbose, TEXT("[ReactionMove] %s frame %d %s velocity (%.0f %.0f %.0f) frames %d"), *GetOwner()->GetName(), GetSimulationFrame(),
		bLaunch ? TEXT("launch") : TEXT("push"), ReactionMove.Velocity.X, ReactionMove.Velocity.Y, ReactionMove.Velocity.Z, ReactionMove.Frames);
	if (!Effect)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Effect, 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, ReactionMove.Velocity.X);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, ReactionMove.Velocity.Y);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, ReactionMove.Velocity.Z);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, ReactionMove.DecayPerFrame);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, ReactionMove.GravityScale);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDeceleration, bLaunch ? ReactionMove.Deceleration : 0.f);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, ReactionMove.Frames * StepMs * 0.001f);
	if (bLaunch && ReactionMove.BalloonFrames > 0)
	{
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveBalloonFrames, static_cast<float>(ReactionMove.BalloonFrames));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveBalloonM0, ReactionMove.BalloonM0);
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveBalloonEnd, ReactionMove.BalloonEndFraction);
	}
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

float UGSTN_CombatSimComponent::GetRunningMoveAirControl() const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline)
	{
		return -1.f;
	}
	const FGSTN_CombatTimelineData* Running = GetRegistryTimeline(Tables(), Sync->Core.ActiveTimeline);
	if (!Running)
	{
		return -1.f;
	}
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Running->Tracks)
	{
		const FGSTN_TimelineTrack* Base = Track.GetPtr<FGSTN_TimelineTrack>();
		if (Base && Base->GetKind() == EGSTN_TimelineTrackKind::Movement)
		{
			return static_cast<const FGSTN_TimelineTrack_Movement*>(Base)->AirControlOverride;
		}
	}
	return -1.f;
}

bool UGSTN_CombatSimComponent::IsTumbleAirRecoveryOn() const
{
	return FGSTN_TumbleRecoveryResolved::IsAfterHitstun(CombatRules ? &CombatRules->TumbleRecovery : nullptr, TumbleRecoveryOverride);
}

const FGameplayTagContainer& UGSTN_CombatSimComponent::GetTumbleRecoveryCommands() const
{
	return FGSTN_TumbleRecoveryResolved::GetAllowedCommands(CombatRules ? &CombatRules->TumbleRecovery : nullptr, KitTumbleRecovery.IsSet() ? &KitTumbleRecovery.GetValue() : nullptr);
}

void UGSTN_CombatSimComponent::ExecuteMeterGain(int32 Amount)
{
	const UClass* Class = CombatRules && !CombatRules->MeterGainEffect.IsNull() ? CombatRules->MeterGainEffect.LoadSynchronous() : nullptr;
	if (!AbilitySystem || !Class || !Class->IsChildOf(UGameplayEffect::StaticClass()) || Amount <= 0)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(const_cast<UClass*>(Class), 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_Meter, static_cast<float>(Amount));
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
