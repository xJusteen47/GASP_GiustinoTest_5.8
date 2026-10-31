// Links, Network Prediction half.

#include "Pipeline/GSTN_HitStep_GrabImmunity.h"
#include "Pipeline/GSTN_HitStep_LinkBreakOnHit.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"

#include "Data/GSTN_LinkHitData.h"
#include "Events/GSTN_SimEvent.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkMash.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "State/GSTN_LinkMath.h"

namespace
{
	/** The weight a link request reads: 100 x the snapshot's weight scale, the same number the interact provider's pre-filter reads */
	float RequestWeight(const FGSTN_HitVictimView& Victim)
	{
		return 100.f * static_cast<float>(Victim.ReactionState.WeightScaleQ) / 64.f;
	}
}

// ---- Helpers

bool FGSTN_LinkStepUtils::IsGrabHit(const FGameplayTag& HitType)
{
	return HitType.IsValid() && HitType.MatchesTag(GSTNCombatHitTags::HitType_Grab);
}

bool FGSTN_LinkStepUtils::IsPartnerHit(const FGameplayTag& HitType)
{
	return HitType.IsValid() && (HitType.MatchesTag(GSTNLinkTags::HitType_Throw) || HitType.MatchesTag(GSTNLinkTags::HitType_Pummel));
}

void FGSTN_LinkStepUtils::MakeRequestContext(const FGSTN_HitContextView& View, FGSTN_LinkRequestContext& OutContext)
{
	MakeRequestContext(View, View.Incoming.Hit.LinkType, OutContext);
}

void FGSTN_LinkStepUtils::MakeRequestContext(const FGSTN_HitContextView& View, const FGameplayTag& LinkType, FGSTN_LinkRequestContext& OutContext)
{
	const FGSTN_ResolvedHit& Incoming = View.Incoming;
	const FGSTN_LinkSnapshotExt* TargetExt = FGSTN_LinkSnapshotExt::FromVictim(View.Victim);
	const FGSTN_LinkSnapshotExt* InitiatorExt = FGSTN_LinkSnapshotExt::FromAttacker(Incoming);

	OutContext.Frame = Incoming.Frame;
	OutContext.LinkType = LinkType;

	FGSTN_LinkRequestSide& Initiator = OutContext.Initiator;
	Initiator.EntityId = Incoming.AttackerEntityId;
	if (InitiatorExt)
	{
		FGSTN_LinkRequestTags::AppendTags(InitiatorExt->RequestTagMask, Initiator.Tags);
		Initiator.GrabImmunityEndFrame = InitiatorExt->GrabImmunityEndFrame;
		Initiator.CooldownEndFrame = InitiatorExt->CooldownEndFrame;
		Initiator.Links = InitiatorExt->ToLinkStates();
		// The initiator's own predicted request for this target is this very request, not a busy channel
		for (FGSTN_LinkState* State : { &Initiator.Links.Body, &Initiator.Links.Hand })
		{
			if (State->Phase == EGSTN_LinkPhase::Pending && !State->PartnerId.IsNull() && static_cast<int32>(State->PartnerId.Id) == View.Victim.EntityId)
			{
				State->Reset();
			}
		}
	}

	FGSTN_LinkRequestSide& Target = OutContext.Target;
	Target.EntityId = View.Victim.EntityId;
	Target.EffectiveWeight = RequestWeight(View.Victim);
	if (TargetExt)
	{
		FGSTN_LinkRequestTags::AppendTags(TargetExt->RequestTagMask, Target.Tags);
		Target.GrabImmunityEndFrame = TargetExt->GrabImmunityEndFrame;
		Target.Links = TargetExt->ToLinkStates();
	}

	if (const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get())
	{
		UGSTN_LinkRequirementLibrary::ResolveDefinitions(*Registry, OutContext);
	}
	const FGSTN_LinkBodyData* InitiatorBody = InitiatorExt ? FGSTN_LinkAttach::At(InitiatorExt->BodyIndex) : nullptr;
	UGSTN_LinkRequirementLibrary::ApplyKitRules(InitiatorBody ? InitiatorBody->GetKitExtension() : nullptr, OutContext);
}

bool FGSTN_LinkStepUtils::ArePartners(const FGSTN_HitContextView& View)
{
	const FGSTN_LinkSnapshotExt* TargetExt = FGSTN_LinkSnapshotExt::FromVictim(View.Victim);
	if (!TargetExt || TargetExt->BodyPhase != static_cast<uint8>(EGSTN_LinkPhase::Active) || TargetExt->PartnerId != View.Incoming.AttackerEntityId)
	{
		return false;
	}
	for (const FInstancedStruct& Extension : View.Incoming.Hit.ExtensionData)
	{
		if (const FGSTN_LinkPartnerHit* Direct = Extension.GetPtr<FGSTN_LinkPartnerHit>())
		{
			return TargetExt->BodySerial == Direct->Serial && TargetExt->BodyTypeIndex == Direct->LinkTypeIndex;
		}
	}
	const FGSTN_LinkSnapshotExt* InitiatorExt = FGSTN_LinkSnapshotExt::FromAttacker(View.Incoming);
	return InitiatorExt && InitiatorExt->BodyPhase == static_cast<uint8>(EGSTN_LinkPhase::Active) && InitiatorExt->PartnerId == View.Victim.EntityId
		&& TargetExt->BodySerial == InitiatorExt->BodySerial && TargetExt->BodyTypeIndex == InitiatorExt->BodyTypeIndex;
}

void FGSTN_LinkStepUtils::SuppressReaction(FGSTN_StepResult& Out)
{
	Out.KnockbackScale = 0.f;
	Out.HitstunScale = 0.f;
	Out.StaggerScale = 0.f;
	Out.bNoFlinch = true;
	Out.MaxReaction = EGSTN_HitReaction::StaggerInPlace;
}

namespace
{
	bool HasNoReaction(const FGSTN_HitData& Hit)
	{
		if (Hit.HitType.IsValid() && Hit.HitType.MatchesTag(GSTNLinkTags::HitType_Pummel))
		{
			return true;
		}
		for (const FInstancedStruct& Extension : Hit.ExtensionData)
		{
			if (const FGSTN_LinkHitData* Data = Extension.GetPtr<FGSTN_LinkHitData>())
			{
				return Data->bNoReaction;
			}
		}
		return false;
	}

	/** The grab rules of the hit's attacker side: its kit's link extension, else the link type's own grab rule */
	const FGSTN_GrabRules* AttackerGrabRules(const FGSTN_HitContextView& View, const UGSTN_LinkTypeDefinition* Definition)
	{
		const FGSTN_LinkSnapshotExt* InitiatorExt = FGSTN_LinkSnapshotExt::FromAttacker(View.Incoming);
		const FGSTN_LinkBodyData* Body = InitiatorExt ? FGSTN_LinkAttach::At(InitiatorExt->BodyIndex) : nullptr;
		return FGSTN_LinkThrow::FindGrabRules(Definition, Body ? Body->GetKitExtension() : nullptr);
	}
}

// ---- GrabImmunity

EGSTN_StepResult UGSTN_HitStep_GrabImmunity::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit || !FGSTN_LinkStepUtils::IsGrabHit(View.Incoming.Hit.HitType))
	{
		return EGSTN_StepResult::Continue;
	}
	const FGSTN_LinkSnapshotExt* Ext = FGSTN_LinkSnapshotExt::FromVictim(View.Victim);
	if (!Ext)
	{
		return EGSTN_StepResult::Continue;
	}
	FGameplayTagContainer Tags;
	FGSTN_LinkRequestTags::AppendTags(Ext->RequestTagMask, Tags);
	const bool bImmuneByFrame = View.Incoming.Frame != INDEX_NONE && View.Incoming.Frame < Ext->GrabImmunityEndFrame;
	if (bImmuneByFrame || Tags.HasTagExact(GSTNLinkTags::Immunity_Grab))
	{
		// No registration: the grab passes through as if it never touched
		Out.bHit = false;
		return EGSTN_StepResult::Stop;
	}
	return EGSTN_StepResult::Continue;
}

// ---- LinkRequest

EGSTN_StepResult UGSTN_HitStep_LinkRequest::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit)
	{
		return EGSTN_StepResult::Continue;
	}
	const FGSTN_HitData& Hit = View.Incoming.Hit;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	// The hit's link filter: the combat core carries it, the link plugin gives it its meaning (a pummel lands only on the held partner, a move
	// may exclude it). The attacker's prediction runs this step on the same snapshot, so a filtered hit never happened on either side
	const bool bPartners = FGSTN_LinkStepUtils::ArePartners(View);
	if ((Hit.LinkFilter == EGSTN_LinkFilter::LinkedPartnerOnly && !bPartners) || (Hit.LinkFilter == EGSTN_LinkFilter::ExcludeLinkedPartner && bPartners))
	{
		Out.bHit = false;
		return EGSTN_StepResult::Stop;
	}

	// A throw or a pummel lands only on the Body partner of the same live link
	if (FGSTN_LinkStepUtils::IsPartnerHit(Hit.HitType))
	{
		if (!bPartners)
		{
			Out.bHit = false;
			return EGSTN_StepResult::Stop;
		}
		const FGSTN_LinkSnapshotExt* TargetExt = FGSTN_LinkSnapshotExt::FromVictim(View.Victim);
		const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(TargetExt->BodyTypeIndex) : nullptr;
		const FGSTN_GrabRules* Rules = AttackerGrabRules(View, Definition);
		if (Hit.HitType.MatchesTag(GSTNLinkTags::HitType_Pummel))
		{
			// No pummel right after the grab (no instant mash): it whiffs until PummelFreeFrames after the formation
			const int32 FreeFrames = Rules ? FMath::Max(Rules->PummelFreeFrames, 0) : 0;
			if (View.Incoming.Frame != INDEX_NONE && View.Incoming.Frame < TargetExt->LinkFrame + FreeFrames)
			{
				Out.bHit = false;
				return EGSTN_StepResult::Stop;
			}
		}
		if (HasNoReaction(Hit))
		{
			FGSTN_LinkStepUtils::SuppressReaction(Out);
		}
		if (Hit.HitType.MatchesTag(GSTNLinkTags::HitType_Throw))
		{
			// The thrown side leaves its link on its next frame, at the thrower's socket where its parked placement is (the snapshot position),
			// facing the thrower; its breather comes from the thrower's grab rules. The knockback steps that follow launch it
			FGSTN_LinkThrowOutcome Outcome;
			Outcome.Serial = TargetExt->BodySerial;
			Outcome.ExitFrame = View.Incoming.Frame != INDEX_NONE ? View.Incoming.Frame + 1 : 0;
			Outcome.ImmunityEndFrame = Rules && Outcome.ExitFrame != 0 ? Outcome.ExitFrame + FMath::Max(Rules->ImmunityFramesAfterThrow, 0) : 0;
			Outcome.ExitPosition = FVector3f(View.Victim.Location);
			Outcome.ExitYawDeg = static_cast<float>(FRotator::NormalizeAxis((-View.Incoming.AttackerFacing).Rotation().Yaw));
			Out.OutcomeExtensions.Add(FInstancedStruct::Make(Outcome));
		}
		return EGSTN_StepResult::Continue;
	}
	if (bPartners && HasNoReaction(Hit))
	{
		// A hit on the partner that opts out of the reaction (a latch attack must not knock the ridden enemy off the latcher)
		FGSTN_LinkStepUtils::SuppressReaction(Out);
	}

	const uint8 TypeIndex = Registry && Hit.LinkType.IsValid() ? Registry->GetTypeIndex(Hit.LinkType) : 0;
	if (TypeIndex == 0)
	{
		return EGSTN_StepResult::Continue;
	}

	// The type the hit names, after the alternatives its definition lists (the grab lists the latch): the first whose checks accept forms
	FGSTN_LinkRequestContext Request;
	FGSTN_LinkRuleResult Decision = FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::NoDefinition);
	const UGSTN_LinkTypeDefinition* Asked = Registry->GetDefinition(TypeIndex);
	TArray<FGameplayTag, TInlineAllocator<4>> Candidates;
	if (Asked)
	{
		Candidates.Append(Asked->Alternatives);
	}
	Candidates.Add(Hit.LinkType);
	for (const FGameplayTag& Candidate : Candidates)
	{
		if (Registry->GetTypeIndex(Candidate) == 0)
		{
			continue;
		}
		Request = FGSTN_LinkRequestContext();
		FGSTN_LinkStepUtils::MakeRequestContext(View, Candidate, Request);
		Decision = UGSTN_LinkRequirementLibrary::EvaluateRequest(Request);
		if (Decision.bAccepted && Request.Definition)
		{
			break;
		}
	}
	if (!Decision.bAccepted || !Request.Definition)
	{
		// The request whiffs on both sides (the initiator's prediction reads the same snapshot): no registration, no hitstop. A predicted request
		// of the initiator that the target refuses on a disagreement clears by RequestTimeoutFrames on every machine alike
		Out.bHit = false;
		return EGSTN_StepResult::Stop;
	}

	const EGSTN_LinkChannel Channel = Request.Definition->Channel;
	const uint8 FormedIndex = Registry->GetTypeIndex(Request.LinkType);
	FGSTN_LinkHitOutcome Outcome;
	Outcome.bAccepted = true;
	Outcome.LinkTypeIndex = FormedIndex;
	Outcome.Channel = Channel;
	Outcome.Serial = FGSTN_LinkState::NextSerial(Request.Target.Links.Get(Channel).Serial, Request.Initiator.Links.Get(Channel).Serial);
	Outcome.LinkFrame = View.Incoming.Frame != INDEX_NONE ? View.Incoming.Frame + 1 : INDEX_NONE;
	Outcome.InitiatorId = View.Incoming.AttackerEntityId;
	// A target parked without acting (the grabbed) mashes out by the grab rules: its end frame from the formation on, the same on every machine
	const FGSTN_LinkRoleDef& TargetRole = Request.Definition->GetRoleDef(2);
	if (TargetRole.bParked && !TargetRole.bActsWhileLinked && Outcome.LinkFrame != INDEX_NONE)
	{
		if (const FGSTN_GrabRules* Rules = AttackerGrabRules(View, Request.Definition))
		{
			Outcome.EndFrame = FGSTN_LinkMash::InitialEndFrame(Rules->Mash, Outcome.LinkFrame, View.Victim.ReactionState.GetDamagePercent());
		}
	}
	Out.OutcomeExtensions.Add(FInstancedStruct::Make(Outcome));

	FGSTN_LinkResponseEntry Response;
	Response.bAccepted = true;
	Response.LinkTypeIndex = FormedIndex;
	Response.Serial = Outcome.Serial;
	Response.LinkFrame = Outcome.LinkFrame;
	Out.OutgoingEntries.Add(FGSTN_LinkInboxHandlers::MakeResponse(Response));

	// A link request does no damage and no knockback: the later steps do not run
	Out.bNoDamage = true;
	return EGSTN_StepResult::Stop;
}

// ---- LinkBreakOnHit

bool UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(const UGSTN_LinkTypeDefinition& Definition, uint8 VictimRole, bool bFromPartner, float Knockback)
{
	// The break evaluator with only the hit filled in: the one place the hit rule is written (UGSTN_LinkMath::EvaluateBreak)
	FGSTN_LinkBreakInput Input;
	Input.OwnRoleTag = Definition.GetRoleDef(VictimRole).RoleTag;
	Input.Hit.bHit = true;
	Input.Hit.bFromPartner = bFromPartner;
	Input.Hit.Knockback = Knockback;
	const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Definition.Break, 0, Input);
	return Decision.bBreak && Decision.Reason == EGSTN_LinkBreakReason::Hit;
}

EGSTN_StepResult UGSTN_HitStep_LinkBreakOnHit::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit || View.Outcome.bBlocked || View.Outcome.bParried)
	{
		return EGSTN_StepResult::Continue;
	}
	const FGSTN_LinkSnapshotExt* Ext = FGSTN_LinkSnapshotExt::FromVictim(View.Victim);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Ext || !Registry || Ext->BodyPhase != static_cast<uint8>(EGSTN_LinkPhase::Active) || Ext->BodyRole == 0)
	{
		return EGSTN_StepResult::Continue;
	}
	const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(Ext->BodyTypeIndex);
	if (!Definition)
	{
		return EGSTN_StepResult::Continue;
	}
	const bool bFromPartner = Ext->PartnerId != INDEX_NONE && Ext->PartnerId == View.Incoming.AttackerEntityId;
	const float Knockback = View.Outcome.bHasKnockback ? View.Outcome.Knockback.Knockback : 0.f;
	if (QualifiesForBreak(*Definition, Ext->BodyRole, bFromPartner, Knockback))
	{
		FGSTN_LinkBreakOutcome Outcome;
		Outcome.Channel = EGSTN_LinkChannel::Body;
		Outcome.Serial = Ext->BodySerial;
		Outcome.Reason = static_cast<uint8>(EGSTN_LinkBreakReason::Hit);
		Out.OutcomeExtensions.Add(FInstancedStruct::Make(Outcome));
	}
	return EGSTN_StepResult::Continue;
}

// ---- Break applier

const UScriptStruct* FGSTN_LinkBreakOutcomeApplier::GetExtensionStruct() const
{
	return FGSTN_LinkBreakOutcome::StaticStruct();
}

void FGSTN_LinkBreakOutcomeApplier::Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const
{
	const FGSTN_LinkBreakOutcome* Outcome = Extension.GetPtr<FGSTN_LinkBreakOutcome>();
	FGSTN_LinkFragment* Fragment = Outcome && Context.VictimFragments ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(*Context.VictimFragments) : nullptr;
	if (!Fragment)
	{
		return;
	}
	const FGSTN_LinkState& State = Fragment->Get(Outcome->Channel);
	if (State.IsActive() && State.Serial == Outcome->Serial && Outcome->Channel == EGSTN_LinkChannel::Body)
	{
		FGSTN_LinkTickExtension::MarkPendingBreak(*Fragment, Outcome->Reason);
	}
}

// ---- Applier

const UScriptStruct* FGSTN_LinkOutcomeApplier::GetExtensionStruct() const
{
	return FGSTN_LinkHitOutcome::StaticStruct();
}

void FGSTN_LinkOutcomeApplier::Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const
{
	const FGSTN_LinkHitOutcome* Outcome = Extension.GetPtr<FGSTN_LinkHitOutcome>();
	if (!Outcome || !Context.VictimFragments)
	{
		return;
	}
	FGSTN_LinkFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(*Context.VictimFragments);
	if (!Outcome->bAccepted)
	{
		Fragment.Actions.LastRefusedSerial = Outcome->Serial;
		Fragment.Actions.RefusedFrame = Outcome->LinkFrame;
		return;
	}
	FGSTN_LinkState& State = Fragment.Get(Outcome->Channel);
	if (State.Phase != EGSTN_LinkPhase::None || !State.IsFormationNew(Outcome->Serial))
	{
		// The channel was taken by an earlier entry of this drain (its step already saw it busy), or the outcome is not newer
		return;
	}
	FGSTN_LinkState Formed;
	Formed.LinkTypeIndex = Outcome->LinkTypeIndex;
	Formed.Role = 2;
	Formed.Phase = EGSTN_LinkPhase::Active;
	Formed.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(Outcome->InitiatorId));
	Formed.Serial = Outcome->Serial;
	Formed.LinkFrame = Outcome->LinkFrame;
	Formed.EndFrame = Outcome->EndFrame;
	State = Formed;
}

void FGSTN_LinkOutcomeApplier::AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const
{
	const FGSTN_LinkHitOutcome* Outcome = Extension.GetPtr<FGSTN_LinkHitOutcome>();
	if (!Outcome || !Outcome->bAccepted)
	{
		return;
	}
	FGSTN_SimEvent Event = Envelope;
	Event.EventType = GSTNLinkTags::Event_LinkFormed;
	Event.Magnitude = static_cast<float>(Outcome->LinkTypeIndex);
	Outbox.Add(MoveTemp(Event));
}
