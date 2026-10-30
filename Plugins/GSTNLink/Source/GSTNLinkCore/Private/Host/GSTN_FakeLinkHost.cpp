// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Host/GSTN_FakeLinkHost.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Placement/GSTN_LinkExitPlacer.h"
#include "Rules/GSTN_LinkRule.h"
#include "State/GSTN_LinkMath.h"
#include "Sync/GSTN_SyncFragment.h"

FGSTN_EntityRef FGSTN_FakeLinkHost::AddFighter(const FGameplayTagContainer& Tags, float EffectiveWeight, const FTransform& Transform)
{
	FEntity& Entity = Entities.AddDefaulted_GetRef();
	Entity.Ref = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(Entities.Num() - 1), 1);
	Entity.Tags = Tags;
	Entity.EffectiveWeight = EffectiveWeight;
	Entity.Transform = Transform;
	return Entity.Ref;
}

FGSTN_FakeLinkHost::FEntity* FGSTN_FakeLinkHost::FindEntity(const FGSTN_EntityRef& Ref)
{
	for (FEntity& Entity : Entities)
	{
		if (Entity.Ref == Ref)
		{
			return &Entity;
		}
	}
	return nullptr;
}

const FGSTN_FakeLinkHost::FEntity* FGSTN_FakeLinkHost::FindEntity(const FGSTN_EntityRef& Ref) const
{
	return const_cast<FGSTN_FakeLinkHost*>(this)->FindEntity(Ref);
}

uint8 FGSTN_FakeLinkHost::RegisterType(const UGSTN_LinkTypeDefinition& Definition)
{
	Definitions.Add(&Definition);
	return static_cast<uint8>(Definitions.Num());
}

const UGSTN_LinkTypeDefinition* FGSTN_FakeLinkHost::GetLinkDefinition(uint8 LinkTypeIndex) const
{
	return LinkTypeIndex >= 1 && LinkTypeIndex <= Definitions.Num() ? Definitions[LinkTypeIndex - 1] : nullptr;
}

bool FGSTN_FakeLinkHost::GetEntityView(const FGSTN_EntityRef& Entity, FGSTN_LinkEntityView& OutView) const
{
	const FEntity* Found = FindEntity(Entity);
	if (Found == nullptr)
	{
		return false;
	}
	OutView.Ref = Found->Ref;
	OutView.Tags = Found->Tags;
	OutView.EffectiveWeight = Found->EffectiveWeight;
	OutView.DamagePercent = Found->DamagePercent;
	OutView.Transform = Found->Transform;
	OutView.bAlive = Found->bAlive;
	OutView.GrabImmunityEndFrame = Found->GrabImmunityEndFrame;
	OutView.LatchImmunityEndFrame = Found->LatchImmunityEndFrame;
	OutView.Links = Found->Links;
	return true;
}

void FGSTN_FakeLinkHost::SetLinkState(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, const FGSTN_LinkState& State)
{
	if (FEntity* Found = FindEntity(Entity))
	{
		Found->Links.Get(Channel) = State;
	}
}

void FGSTN_FakeLinkHost::SetImmunity(const FGSTN_EntityRef& Entity, EGSTN_LinkImmunityKind Kind, int32 EndFrame)
{
	if (FEntity* Found = FindEntity(Entity))
	{
		(Kind == EGSTN_LinkImmunityKind::Grab ? Found->GrabImmunityEndFrame : Found->LatchImmunityEndFrame) = EndFrame;
	}
}

void FGSTN_FakeLinkHost::PlaceEntity(const FGSTN_EntityRef& Entity, const FTransform& Transform, const FVector& Velocity)
{
	if (FEntity* Found = FindEntity(Entity))
	{
		Found->Transform = Transform;
		Found->Velocity = Velocity;
	}
}

FGSTN_LinkRuleResult FGSTN_FakeLinkHost::RequestLink(const FGSTN_EntityRef& Initiator, const FGSTN_EntityRef& Target, uint8 LinkTypeIndex, bool bApplyMash)
{
	const UGSTN_LinkTypeDefinition* Definition = GetLinkDefinition(LinkTypeIndex);
	FEntity* InitiatorEntity = FindEntity(Initiator);
	FEntity* TargetEntity = FindEntity(Target);
	if (Definition == nullptr || InitiatorEntity == nullptr || TargetEntity == nullptr)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::NoDefinition);
	}

	const EGSTN_LinkChannel Channel = Definition->Channel;
	const EGSTN_LinkChannel OtherChannel = Channel == EGSTN_LinkChannel::Body ? EGSTN_LinkChannel::Hand : EGSTN_LinkChannel::Body;
	const auto MakeSide = [this, Channel, OtherChannel](const FEntity& Entity)
	{
		FGSTN_LinkRequestSide Side;
		Side.EntityId = Entity.Ref.Id;
		Side.Tags = Entity.Tags;
		Side.EffectiveWeight = Entity.EffectiveWeight;
		Side.GrabImmunityEndFrame = Entity.GrabImmunityEndFrame;
		Side.LatchImmunityEndFrame = Entity.LatchImmunityEndFrame;
		Side.Links = Entity.Links;
		const FGSTN_LinkState& Other = Entity.Links.Get(OtherChannel);
		if (Other.Phase != EGSTN_LinkPhase::None)
		{
			Side.OtherChannelDefinition = GetLinkDefinition(Other.LinkTypeIndex);
		}
		return Side;
	};

	FGSTN_LinkRequestContext Context;
	Context.Frame = Frame;
	Context.LinkType = Definition->LinkType;
	Context.Initiator = MakeSide(*InitiatorEntity);
	Context.Target = MakeSide(*TargetEntity);
	Context.Definition = Definition;
	Context.InitiatorGrabRules = &GrabRules;
	Context.InitiatorLatchRules = &LatchRules;

	// The shared checks first, then the definition's rules in order: the first refusal wins
	FGSTN_LinkRuleResult Verdict = UGSTN_LinkRule::CheckCommon(Context);
	for (int32 RuleIndex = 0; Verdict.bAccepted && RuleIndex < Definition->Rules.Num(); ++RuleIndex)
	{
		if (const UGSTN_LinkRule* Rule = Definition->Rules[RuleIndex])
		{
			Verdict = Rule->Evaluate(Context);
		}
	}
	if (!Verdict.bAccepted)
	{
		return Verdict;
	}

	const FGSTN_LinkState& InitiatorSlot = InitiatorEntity->Links.Get(Channel);
	const FGSTN_LinkState& TargetSlot = TargetEntity->Links.Get(Channel);
	const uint8 Serial = FGSTN_LinkState::NextSerial(InitiatorSlot.Serial, TargetSlot.Serial);

	FGSTN_LinkState InitiatorState;
	InitiatorState.LinkTypeIndex = LinkTypeIndex;
	InitiatorState.Role = 1;
	InitiatorState.Phase = EGSTN_LinkPhase::Active;
	InitiatorState.PartnerId = Target;
	InitiatorState.Serial = Serial;
	InitiatorState.LinkFrame = Frame;

	FGSTN_LinkState TargetState = InitiatorState;
	TargetState.Role = 2;
	TargetState.PartnerId = Initiator;
	TargetState.EndFrame = bApplyMash ? Frame + UGSTN_LinkMath::MashInitialFrames(GrabRules.Mash, TargetEntity->DamagePercent) : 0;

	InitiatorEntity->Links.Get(Channel) = InitiatorState;
	TargetEntity->Links.Get(Channel) = TargetState;

	// The parked entity stands where the definition derives it from the other one, from the formation on
	const int32 ParkedIndex = Definition->GetParkedRoleIndex();
	if (ParkedIndex != 0)
	{
		FEntity& Parked = ParkedIndex == 1 ? *InitiatorEntity : *TargetEntity;
		const FEntity& Anchor = ParkedIndex == 1 ? *TargetEntity : *InitiatorEntity;
		Parked.Transform = UGSTN_LinkMath::ParkedTransform(Anchor.Transform, Definition->GetRoleDef(ParkedIndex));
		Parked.Velocity = FVector::ZeroVector;
	}

	FGSTN_LinkEventRecord Formed;
	Formed.Event = GSTNLinkTags::Event_LinkFormed;
	Formed.Frame = Frame;
	Formed.Initiator = Initiator;
	Formed.Target = Target;
	Formed.LinkTypeIndex = LinkTypeIndex;
	PublishEvent(Formed);
	return FGSTN_LinkRuleResult::Accept();
}

bool FGSTN_FakeLinkHost::RequestBreak(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, EGSTN_LinkBreakReason Reason)
{
	const FEntity* Found = FindEntity(Entity);
	if (Found == nullptr || !Found->Links.Get(Channel).IsActive())
	{
		return false;
	}
	BreakPair(Entity, Channel, Reason);
	return true;
}

void FGSTN_FakeLinkHost::BreakPair(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, EGSTN_LinkBreakReason Reason)
{
	FEntity* Breaker = FindEntity(Entity);
	if (Breaker == nullptr || !Breaker->Links.Get(Channel).IsLinked())
	{
		return;
	}
	const FGSTN_LinkState BreakerState = Breaker->Links.Get(Channel);
	const FGSTN_EntityRef Initiator = BreakerState.Role == 1 ? Breaker->Ref : BreakerState.PartnerId;
	const FGSTN_EntityRef Target = BreakerState.Role == 1 ? BreakerState.PartnerId : Breaker->Ref;
	const UGSTN_LinkTypeDefinition* Definition = GetLinkDefinition(BreakerState.LinkTypeIndex);

	FGSTN_LinkEventRecord Broken;
	Broken.Event = GSTNLinkTags::Event_LinkBroken;
	Broken.Frame = Frame;
	Broken.Initiator = Initiator;
	Broken.Target = Target;
	Broken.LinkTypeIndex = BreakerState.LinkTypeIndex;
	Broken.Reason = Reason;
	PublishEvent(Broken);
	if (Reason == EGSTN_LinkBreakReason::Escape)
	{
		FGSTN_LinkEventRecord Escaped = Broken;
		Escaped.Event = GSTNLinkTags::Event_GrabEscaped;
		PublishEvent(Escaped);
	}

	if (Definition != nullptr)
	{
		PlaceParkedEntity(*Definition, Initiator, Target);

		// The entity that was linked to has a breather before it can be linked to again
		if (Definition->LinkType == GSTNLinkTags::Type_Grab.GetTag())
		{
			const int32 Frames = Reason == EGSTN_LinkBreakReason::Throw ? GrabRules.ImmunityFramesAfterThrow : GrabRules.ImmunityFramesAfterRelease;
			SetImmunity(Target, EGSTN_LinkImmunityKind::Grab, Frame + Frames);
		}
		else if (Definition->LinkType == GSTNLinkTags::Type_Latch.GetTag())
		{
			SetImmunity(Target, EGSTN_LinkImmunityKind::Latch, Frame + LatchRules.ImmunityFramesAfterLatch);
		}
	}

	// Both slots clear together, but a partner whose slot is another link (an orphan's) is left alone
	Breaker->Links.Get(Channel).Reset();
	if (FEntity* Partner = FindEntity(BreakerState.PartnerId))
	{
		FGSTN_LinkState& PartnerSlot = Partner->Links.Get(Channel);
		if (PartnerSlot.IsLinked() && PartnerSlot.Serial == BreakerState.Serial && PartnerSlot.PartnerId == Breaker->Ref)
		{
			PartnerSlot.Reset();
		}
	}
}

void FGSTN_FakeLinkHost::PlaceParkedEntity(const UGSTN_LinkTypeDefinition& Definition, const FGSTN_EntityRef& Initiator, const FGSTN_EntityRef& Target)
{
	const int32 ParkedIndex = Definition.GetParkedRoleIndex();
	if (ParkedIndex == 0)
	{
		return;
	}
	const FGSTN_EntityRef ParkedRef = ParkedIndex == 1 ? Initiator : Target;
	const FGSTN_EntityRef AnchorRef = ParkedIndex == 1 ? Target : Initiator;
	const FEntity* Anchor = FindEntity(AnchorRef);
	if (Anchor == nullptr || FindEntity(ParkedRef) == nullptr)
	{
		return;
	}

	FGSTN_ExitQuery Query;
	Query.PartnerTransform = Anchor->Transform;
	Query.PreferredDirection = Anchor->Transform.GetUnitAxis(EAxis::X);
	Query.Collision = ExitCollision;
	const FGSTN_ExitResult Exit = Definition.GetExitPlacer()->Place(Query);
	PlaceEntity(ParkedRef, Exit.Transform, Exit.Velocity);
}

FGSTN_FakeLinkHost::FStepResult FGSTN_FakeLinkHost::Step()
{
	FStepResult Result;
	const int32 Grace = OrphanGraceFrames >= 0 ? OrphanGraceFrames : UGSTN_LinkSettings::Get().OrphanGraceFrames;

	for (int32 EntityIndex = 0; EntityIndex < Entities.Num(); ++EntityIndex)
	{
		for (const EGSTN_LinkChannel Channel : { EGSTN_LinkChannel::Body, EGSTN_LinkChannel::Hand })
		{
			const FEntity& Entity = Entities[EntityIndex];
			const FGSTN_LinkState State = Entity.Links.Get(Channel);
			if (!State.IsActive())
			{
				continue;
			}

			// The pair is consistent when the partner holds the other end of the very same link
			const FEntity* Partner = FindEntity(State.PartnerId);
			const FGSTN_LinkState* PartnerState = Partner != nullptr ? &Partner->Links.Get(Channel) : nullptr;
			if (PartnerState == nullptr || !PartnerState->IsLinked() || PartnerState->Serial != State.Serial || PartnerState->PartnerId != Entity.Ref)
			{
				if (Frame - State.LinkFrame >= Grace)
				{
					++Result.Orphans;
					++Result.Breaks;
					BreakPair(Entity.Ref, Channel, EGSTN_LinkBreakReason::Forced);
				}
				continue;
			}

			const UGSTN_LinkTypeDefinition* Definition = GetLinkDefinition(State.LinkTypeIndex);
			if (Definition == nullptr)
			{
				continue;
			}

			FGSTN_LinkBreakInput Input;
			Input.Frame = Frame;
			Input.LinkFrame = State.LinkFrame;
			Input.EndFrame = State.EndFrame;
			Input.bPendingBreak = State.HasFlag(GSTNLinkFlags::PendingBreak);
			Input.PendingReason = State.GetBreakReason();
			Input.OwnRoleTag = Definition->GetRoleDef(State.Role).RoleTag;
			Input.bOwnDead = !Entity.bAlive;
			Input.bPartnerDead = !Partner->bAlive;
			Input.PartnerStateTags = Partner->Tags;
			Input.Hit = Entity.PendingHit;

			// How far the parked entity stands from where the link expects it
			const int32 ParkedIndex = Definition->GetParkedRoleIndex();
			if (ParkedIndex != 0)
			{
				const FEntity& Parked = State.Role == ParkedIndex ? Entity : *Partner;
				const FEntity& Anchor = State.Role == ParkedIndex ? *Partner : Entity;
				const FTransform Expected = UGSTN_LinkMath::ParkedTransform(Anchor.Transform, Definition->GetRoleDef(ParkedIndex));
				Input.PartnerDistanceCm = static_cast<float>(FVector::Dist(Parked.Transform.GetLocation(), Expected.GetLocation()));
			}

			const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Definition->Break, Definition->MaxFrames, Input);
			if (Decision.bBreak)
			{
				++Result.Breaks;
				BreakPair(Entity.Ref, Channel, Decision.Reason);
			}
		}
	}

	for (FEntity& Entity : Entities)
	{
		Entity.PendingHit = FGSTN_LinkBreakHit();
	}
	return Result;
}
