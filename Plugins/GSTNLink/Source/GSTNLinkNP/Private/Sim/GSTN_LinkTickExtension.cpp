// Links, Network Prediction half.

#include "Sim/GSTN_LinkTickExtension.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_InteractMath.h"
#include "GSTN_InteractTags.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Movement/GSTN_LinkExitRequest.h"
#include "Movement/GSTN_LinkedModifier.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkEffects.h"
#include "Sim/GSTN_LinkExitCollision.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkMash.h"
#include "Sim/GSTN_LinkShakeOff.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

namespace
{
	const UGSTN_LinkTypeDefinition* BodyDefinition(const FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry)
	{
		return Registry ? Registry->GetDefinition(Fragment.Links.Body.LinkTypeIndex) : nullptr;
	}

}

FGSTN_LinkRequestSide FGSTN_LinkTickExtension::MakeRequestSide(const FGSTN_SnapshotEntry& Entry)
{
	FGSTN_LinkRequestSide Side;
	Side.EntityId = Entry.EntityId;
	Side.TeamId = Entry.Team.GetId();
	Side.EffectiveWeight = 100.f * static_cast<float>(Entry.Reaction.WeightScaleQ) / 64.f;
	const FGSTN_LinkSnapshotExt* Ext = Entry.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	if (Ext && Ext->IsValid())
	{
		FGSTN_LinkRequestTags::AppendTags(Ext->RequestTagMask, Side.Tags);
		Side.GrabImmunityEndFrame = Ext->GrabImmunityEndFrame;
		Side.CooldownEndFrame = Ext->CooldownEndFrame;
		Side.Links = Ext->ToLinkStates();
		if (const FGSTN_LinkBodyData* Body = FGSTN_LinkAttach::At(Ext->BodyIndex))
		{
			Side.KitExtensions = Body->GetKitExtensions();
		}
	}
	return Side;
}

uint8 FGSTN_LinkTickExtension::GetTypeId() const
{
	return GSTNFeatureIds::Link;
}

bool FGSTN_LinkTickExtension::StepLinkTick(FGSTN_LinkFragment& Fragment, int32 Frame, int32 RequestTimeoutFrames)
{
	bool bChanged = false;
	for (FGSTN_LinkState* State : { &Fragment.Links.Body, &Fragment.Links.Hand })
	{
		// A predicted link whose response never came (refused on a machine that did not tell this one, or lost): cleared, the serial kept
		if (State->Phase == EGSTN_LinkPhase::Pending && State->HasFlag(GSTNLinkFlags::Predicted)
			&& Frame >= State->LinkFrame + FMath::Max(RequestTimeoutFrames, 1))
		{
			State->Reset();
			bChanged = true;
		}
	}
	return bChanged;
}

bool FGSTN_LinkTickExtension::IsActionBlocked(const FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	const uint8 Bits = FGSTN_LinkSnapshotContributor::ComputeBits(BodyDefinition(Fragment, Registry), Body.Role, Body.Phase);
	return (Bits & GSTNLinkSnapshotBits::BlocksActions) != 0;
}

bool FGSTN_LinkTickExtension::IsParked(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry)
{
	if (!Fragment)
	{
		return false;
	}
	const FGSTN_LinkState& Body = Fragment->Links.Body;
	return (FGSTN_LinkSnapshotContributor::ComputeBits(BodyDefinition(*Fragment, Registry), Body.Role, Body.Phase) & GSTNLinkSnapshotBits::Parked) != 0;
}

bool FGSTN_LinkTickExtension::MarkPredicted(FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry& Registry, const FGameplayTag& LinkType, int32 VictimId, int32 Frame)
{
	const uint8 TypeIndex = Registry.GetTypeIndex(LinkType);
	const UGSTN_LinkTypeDefinition* Definition = Registry.GetDefinition(TypeIndex);
	if (!Definition || VictimId == INDEX_NONE)
	{
		return false;
	}
	FGSTN_LinkState& State = Fragment.Get(Definition->Channel);
	if (State.Phase != EGSTN_LinkPhase::None)
	{
		// One request at a time per channel: a second grab-like hit of the same frame or a held link keeps what is there
		return false;
	}
	const uint8 KeptSerial = State.Serial;
	State = FGSTN_LinkState();
	State.Serial = KeptSerial;
	State.LinkTypeIndex = TypeIndex;
	State.Phase = EGSTN_LinkPhase::Pending;
	State.Flags = GSTNLinkFlags::Predicted;
	State.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(VictimId));
	State.LinkFrame = Frame;
	return true;
}

bool FGSTN_LinkTickExtension::MarkPendingBreak(FGSTN_LinkFragment& Fragment, uint8 Reason)
{
	FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Body.IsLinked() || Body.HasFlag(GSTNLinkFlags::PendingBreak))
	{
		return false;
	}
	Body.Flags |= GSTNLinkFlags::PendingBreak;
	Body.BreakReason = Reason;
	return true;
}

void FGSTN_LinkTickExtension::GatherStateTags(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry, FGameplayTagContainer& Out)
{
	if (!Fragment)
	{
		return;
	}
	if (Fragment->Links.Body.IsLinked())
	{
		Out.AddTag(GSTNLinkTags::Linked);
	}
	for (const FGSTN_LinkState* State : { &Fragment->Links.Body, &Fragment->Links.Hand })
	{
		if (State->Phase == EGSTN_LinkPhase::Pending)
		{
			Out.AddTag(GSTNLinkTags::Pending);
		}
		if (State->IsLinked() && Registry)
		{
			const FGameplayTag RoleTag = Registry->GetRoleTag(State->LinkTypeIndex, State->Role);
			if (RoleTag.IsValid())
			{
				Out.AddTag(RoleTag);
			}
		}
	}
}

void FGSTN_LinkTickExtension::GatherProjectableTags(const UGSTN_LinkRegistry* Registry, FGameplayTagContainer& Out)
{
	Out.AddTag(GSTNLinkTags::Linked);
	Out.AddTag(GSTNLinkTags::Pending);
	if (!Registry)
	{
		return;
	}
	for (int32 Index = 1; Index <= Registry->Num(); ++Index)
	{
		for (int32 Role = 1; Role <= 2; ++Role)
		{
			const FGameplayTag RoleTag = Registry->GetRoleTag(static_cast<uint8>(Index), Role);
			if (RoleTag.IsValid())
			{
				Out.AddTag(RoleTag);
			}
		}
	}
}

namespace
{
	/** The kit link extension of an entity, from its snapshot entry's body index (content the attach table holds); null without one */
	const FGSTN_LinkKitExtension* KitOf(const FGSTN_StepSnapshot* Snapshot, int32 EntityId)
	{
		const FGSTN_SnapshotEntry* Entry = Snapshot && EntityId != INDEX_NONE ? Snapshot->Find(EntityId) : nullptr;
		const FGSTN_LinkSnapshotExt* Ext = Entry ? Entry->Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link) : nullptr;
		const FGSTN_LinkBodyData* Body = Ext && Ext->IsValid() ? FGSTN_LinkAttach::At(Ext->BodyIndex) : nullptr;
		return Body ? Body->GetKitExtension() : nullptr;
	}
}

bool FGSTN_LinkTickExtension::StepOutcomeKeys(const FGSTN_CombatTickContext& Context, FGSTN_LinkFragment& Fragment, TArray<FThrowPost, TInlineAllocator<1>>& OutThrows)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Context.RunningTimeline || !Body.IsActive() || Body.Role == 0)
	{
		return false;
	}
	int32 Previous = 0;
	int32 Current = 0;
	if (!FGSTN_LinkOutcomeKeys::GetCrossedFrames(Context.In.Core, Context.Out.Core, Context.Frame, Previous, Current))
	{
		return false;
	}
	TArray<const FGSTN_LinkOutcomeKey*, TInlineAllocator<2>> Keys;
	FGSTN_LinkOutcomeKeys::Collect(*Context.RunningTimeline, Previous, Current, Keys);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(Body.LinkTypeIndex) : nullptr;
	if (Keys.IsEmpty() || !Definition)
	{
		return false;
	}

	bool bChanged = false;
	for (const FGSTN_LinkOutcomeKey* Key : Keys)
	{
		if (!Fragment.Links.Body.IsActive())
		{
			// An earlier key of the frame ended the link (a throw): the rest of them have nothing to act on
			break;
		}
		switch (Key->Outcome)
		{
		case EGSTN_LinkOutcome::Release:
			bChanged |= FGSTN_LinkShakeOff::ApplyReleaseKey(Fragment, *Definition, Registry);
			break;
		case EGSTN_LinkOutcome::Throw:
		{
			const int32 PartnerId = Fragment.Links.Body.PartnerId.IsNull() ? INDEX_NONE : static_cast<int32>(Fragment.Links.Body.PartnerId.Id);
			const FGSTN_SnapshotEntry* Self = Context.Snapshot && Context.EntityId != INDEX_NONE ? Context.Snapshot->Find(Context.EntityId) : nullptr;
			const FGSTN_SnapshotEntry* Partner = Context.Snapshot && PartnerId != INDEX_NONE ? Context.Snapshot->Find(PartnerId) : nullptr;
			if (!Self || !Partner)
			{
				// Without both entities in the step snapshot there is nobody to hit: the key does nothing (the link stays)
				break;
			}
			FGSTN_LinkThrow::WarnUnresolvedExtensions(Key->ThrowHit);
			const UGSTN_CombatSimComponent* Combat = Context.Owner ? Context.Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
			FThrowPost& Post = OutThrows.AddDefaulted_GetRef();
			Post.PartnerId = PartnerId;
			Post.Entry = FGSTN_LinkThrow::MakeThrowEntry(*Key, Context.Frame, *Self, *Partner, Fragment.Links.Body.Serial, Fragment.Links.Body.LinkTypeIndex,
				Combat ? Combat->GetCombatRules() : nullptr);
			bChanged |= FGSTN_LinkThrow::EndOwnLink(Fragment, Context.Frame, FGSTN_LinkThrow::FindGrabRules(Definition, KitOf(Context.Snapshot, Context.EntityId)));
			break;
		}
		case EGSTN_LinkOutcome::SwapRoles:
		{
			static bool bWarned = false;
			UE_CLOG(!bWarned, LogTemp, Warning, TEXT("A link outcome key asks for SwapRoles, which is not built (M6 F12): ignored"));
			bWarned = true;
			break;
		}
		default:
			break;
		}
	}
	return bChanged;
}

bool FGSTN_LinkTickExtension::StepMash(const FGSTN_CombatTickContext& Context, FGSTN_LinkFragment& Fragment)
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Body.IsActive() || Body.EndFrame <= 0 || !IsActionBlocked(Fragment, Registry))
	{
		return false;
	}
	// The grabber's rules: its kit's link extension (from its snapshot entry), else the type's own grab rule
	const int32 PartnerId = Body.PartnerId.IsNull() ? INDEX_NONE : static_cast<int32>(Body.PartnerId.Id);
	const FGSTN_GrabRules* Rules = FGSTN_LinkThrow::FindGrabRules(Registry ? Registry->GetDefinition(Body.LinkTypeIndex) : nullptr, KitOf(Context.Snapshot, PartnerId));
	if (!Rules)
	{
		return false;
	}
	return FGSTN_LinkMash::Step(Fragment, Context.In.Core.Input.History, Rules->Mash, FGSTN_LinkMash::ButtonMaskOf(Rules->Mash, Context.Buttons), Context.Frame);
}

void FGSTN_LinkTickExtension::MakeOffersFor(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkBodyData* Body, TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>>& OutOffers)
{
	if (Body)
	{
		FGSTN_LinkInteractProvider::MakeOffers(Registry, Body->GetKitExtensions(), Body->GetKitExtension(), OutOffers);
	}
}

bool FGSTN_LinkTickExtension::PredictRequest(FGSTN_LinkState& State, const UGSTN_LinkRegistry& Registry, const FGSTN_SnapshotEntry& Self,
	const FGSTN_SnapshotEntry& Victim, const FGameplayTag& AskedType, int32 Frame)
{
	const UGSTN_LinkTypeDefinition* Asked = Registry.FindDefinition(AskedType);
	if (!Asked)
	{
		return false;
	}
	// The context the target's LinkRequest step builds from the same snapshot (FGSTN_LinkStepUtils::MakeRequestContext), so the initiator holds
	// the type and the payload the target will form
	const FGSTN_LinkSnapshotExt* SelfExt = Self.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	const FGSTN_LinkBodyData* SelfBody = SelfExt && SelfExt->IsValid() ? FGSTN_LinkAttach::At(SelfExt->BodyIndex) : nullptr;
	TArray<FGameplayTag, TInlineAllocator<4>> Candidates(Asked->Alternatives);
	Candidates.Add(AskedType);
	for (const FGameplayTag& Candidate : Candidates)
	{
		FGSTN_LinkRequestContext Request;
		Request.Frame = Frame;
		Request.LinkType = Candidate;
		Request.Initiator = MakeRequestSide(Self);
		Request.Target = MakeRequestSide(Victim);
		UGSTN_LinkRequirementLibrary::ResolveDefinitions(Registry, Request);
		UGSTN_LinkRequirementLibrary::ApplyKitRules(SelfBody ? SelfBody->GetKitExtension() : nullptr, Request);
		if (!Request.Definition || Request.Definition->Channel != Asked->Channel || !UGSTN_LinkRequirementLibrary::EvaluateRequest(Request).bAccepted)
		{
			continue;
		}
		State.LinkTypeIndex = Registry.GetTypeIndex(Candidate);
		State.Payload = UGSTN_LinkRequirementLibrary::ComputePayload(Request);
		return true;
	}
	return false;
}

bool FGSTN_LinkTickExtension::StepOptimisticAccept(FGSTN_LinkFragment& Fragment, int32 SelfId, int32 Frame, const UGSTN_LinkRegistry* Registry,
	TFunctionRef<const FGSTN_LinkSnapshotExt*(int32 EntityId)> TargetExtOf)
{
	if (!Registry)
	{
		return false;
	}
	bool bChanged = false;
	for (const EGSTN_LinkChannel Channel : { EGSTN_LinkChannel::Body, EGSTN_LinkChannel::Hand })
	{
		FGSTN_LinkState& State = Fragment.Get(Channel);
		if (State.Phase != EGSTN_LinkPhase::Pending || !State.HasFlag(GSTNLinkFlags::Predicted) || State.PartnerId.IsNull())
		{
			continue;
		}
		const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(State.LinkTypeIndex);
		if (!Definition || !Definition->bInitiatorPredicts || Frame < State.LinkFrame + FMath::Max(Definition->PendingFrames, 1))
		{
			continue;
		}
		const FGSTN_LinkSnapshotExt* Target = TargetExtOf(static_cast<int32>(State.PartnerId.Id));
		if (!Target || !Target->IsValid())
		{
			// Nothing known about the target on this machine: the request waits for its response or its timeout
			continue;
		}

		uint8 Serial = 0;
		int32 LinkFrame = State.LinkFrame + 1;
		if (Channel == EGSTN_LinkChannel::Body)
		{
			if (Target->BodyPhase != static_cast<uint8>(EGSTN_LinkPhase::None))
			{
				if (Target->BodyRole != 2 || Target->PartnerId != SelfId)
				{
					// The target holds another link (a second initiator came first, or it initiated its own): what a refusal would say
					State.Reset();
					bChanged = true;
					continue;
				}
				// The target formed with this entity already (a machine that simulates it): its serial and frame are the authority's
				Serial = Target->BodySerial;
				LinkFrame = Target->LinkFrame;
			}
			else
			{
				Serial = FGSTN_LinkState::NextSerial(Target->BodySerial, State.Serial);
			}
		}
		else
		{
			// The snapshot carries no Hand serial or partner: a held Hand link is a refusal, a free one predicts from the entity's own serial
			if (Target->HandRole != 0)
			{
				State.Reset();
				bChanged = true;
				continue;
			}
			Serial = FGSTN_LinkState::NextSerial(0, State.Serial);
		}

		State.Role = 1;
		State.Phase = EGSTN_LinkPhase::Active;
		State.Serial = Serial;
		State.LinkFrame = LinkFrame;
		// The Predicted flag stays: the response clears it where it arrives; elsewhere the authority's state equals this one but for the flag
		bChanged = true;
	}
	return bChanged;
}

FGameplayTag FGSTN_LinkTickExtension::ResolveInteractTarget(const FGSTN_SnapshotEntry& Self, TConstArrayView<FGSTN_SnapshotEntry> Entries, int32 Frame,
	const FVector& StickOnPlane) const
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkSnapshotExt* SelfExt = Self.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	if (!Registry || !SelfExt || !SelfExt->IsValid())
	{
		return FGameplayTag();
	}
	// Only a kit that some offer applies to (the grab: a link extension) looks for a target: the probe reads other entities' snapshot positions,
	// which a client knows as interpolated proxies, so its result is not prediction-stable and must not reach a pawn that never uses it (F10)
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>> Offers;
	MakeOffersFor(*Registry, FGSTN_LinkAttach::At(SelfExt->BodyIndex), Offers);
	if (Offers.IsEmpty())
	{
		return FGameplayTag();
	}

	TArray<FGSTN_LinkInteractEntity, TInlineAllocator<16>> Candidates;
	for (const FGSTN_SnapshotEntry& Entry : Entries)
	{
		if (Entry.EntityId == Self.EntityId || !Entry.Hurtboxes)
		{
			continue;
		}
		FGSTN_LinkInteractEntity& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Side = MakeRequestSide(Entry);
		Candidate.Location = Entry.Transform.GetLocation();
		Candidate.RadiusCm = FMath::RoundToInt32(FMath::Min(Entry.BoundRadius, 60.f));
	}
	if (Candidates.IsEmpty())
	{
		return FGameplayTag();
	}

	FGSTN_LinkInteractSource Source;
	Source.Interactor = MakeRequestSide(Self);
	Source.Entities = Candidates;
	Source.Offers = Offers;
	Source.Registry = Registry;

	const FGSTN_InteractMeaningTable& Meanings = Registry->GetInteractMeanings();
	static const FGSTN_InteractRules Rules;
	const IGSTN_InteractCandidateProvider* Providers[] = { &Provider };
	FGSTN_InteractQuery Query;
	Query.Frame = Frame;
	Query.Origin = Self.Transform.GetLocation();
	Query.Up = Self.Up;
	Query.Facing = Self.Facing;
	Query.StickDirection = StickOnPlane;
	Query.StickMagnitude = static_cast<float>(StickOnPlane.Size());
	Query.Tags = &Source.Interactor.Tags;
	Query.Rules = &Rules;
	Query.Meanings = &Meanings;
	Query.Providers = Providers;
	Query.HostContext = Source.ToHostContext();

	// One stateless probe: the focus of this frame only (F10: no sticky focus state in the fragment)
	FGSTN_InteractFocusState State;
	const FGSTN_InteractFocusResult Focus = UGSTN_InteractMath::UpdateFocus(Query, State);
	FGameplayTagContainer Projected;
	FGSTN_LinkInteractProvider::ProjectTargetTag(Focus, Meanings, Projected);
	// ProjectTargetTag leaves at most one Interact.Target tag: the focus meaning
	return Projected.IsEmpty() ? FGameplayTag() : Projected.First();
}

void FGSTN_LinkTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments))
		{
			// M7 G1: a predicting type's request accepts itself PendingFrames after the hit, on the target's entry of the step snapshot (end of the
			// previous frame). The response of a machine that simulates the target was drained before this point, so there it already formed
			if (Context.Snapshot && Context.EntityId != INDEX_NONE)
			{
				const FGSTN_StepSnapshot& Snapshot = *Context.Snapshot;
				StepOptimisticAccept(*Fragment, Context.EntityId, Context.Frame, UGSTN_LinkRegistry::Get(), [&Snapshot](int32 EntityId) -> const FGSTN_LinkSnapshotExt*
				{
					const FGSTN_SnapshotEntry* Entry = Snapshot.Find(EntityId);
					return Entry ? Entry->Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link) : nullptr;
				});
			}
			StepLinkTick(*Fragment, Context.Frame, UGSTN_LinkSettings::Get().RequestTimeoutFrames);
			// The exit a break recorded is read on its frame (F11's exit request); after it the record is dropped, so an idle entity is idle on the wire
			if (Fragment->Actions.Exit.Frame != 0 && Context.Frame > Fragment->Actions.Exit.Frame)
			{
				Fragment->Actions.Exit = FGSTN_LinkActions::FExit();
				// ...and with it the reason the break left on an idle channel (a reason makes the state "live" on the wire: ~20 B forever)
				for (FGSTN_LinkState* State : { &Fragment->Links.Body, &Fragment->Links.Hand })
				{
					if (State->Phase == EGSTN_LinkPhase::None && State->Role == 0)
					{
						State->BreakReason = 0;
					}
				}
			}
			// A grab immunity that has run out is dropped too, for the same reason
			if (Fragment->GrabImmunityEndFrame != 0 && Context.Frame >= Fragment->GrabImmunityEndFrame)
			{
				Fragment->GrabImmunityEndFrame = 0;
			}
			// ...and the exit collision window (F15)
			FGSTN_LinkExitCollision::DropExpired(*Fragment, Context.Frame);
			// The frame a non-acting parked role starts (the victim step or the inbox wrote it this frame): its running move ends, so no hitbox
			// of it hits the partner later this frame
			const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
			const FGSTN_LinkFragment* Before = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.In.Fragments);
			if (IsActionBlocked(*Fragment, Registry) && !(Before && IsActionBlocked(*Before, Registry))
				&& Context.Out.Core.ActionState == EGSTN_ActionState::InTimeline)
			{
				FGSTN_CombatTimelineLogic::CancelForOtherCommand(Context.Out.Core, Context.Frame);
			}
			// F12: the keys of the running move (release, throw, a shake-off), then the grabbed side's mash
			TArray<FThrowPost, TInlineAllocator<1>> Throws;
			StepOutcomeKeys(Context, *Fragment, Throws);
			StepMash(Context, *Fragment);
			if (!Throws.IsEmpty() && Context.Owner)
			{
				// The direct throw hit, posted like the attacker step posts its hits: stamped this frame, delivered at the partner's next. A partner that
				// does not tick on this machine refuses it (the authority always takes it)
				if (UGSTN_CombatWorldSubsystem* World = UGSTN_CombatWorldSubsystem::Get(Context.Owner->GetWorld()))
				{
					for (const FThrowPost& Post : Throws)
					{
						World->PostHit(Post.PartnerId, Post.Entry);
					}
				}
			}
		}
		break;
	case EGSTN_CombatHookPoint::AfterExecute:
		if (!Context.AttackerHits.IsEmpty())
		{
			const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
			for (const FGSTN_AttackerHit& Hit : Context.AttackerHits)
			{
				const FGameplayTag LinkType = Hit.Entry.Hit.LinkType;
				if (Hit.bRicochet || !LinkType.IsValid() || !Registry || Registry->GetTypeIndex(LinkType) == 0)
				{
					continue;
				}
				// The fragment exists from the first link on: a grab-like hit is when it first appears
				FGSTN_LinkFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(Context.Out.Fragments);
				if (!MarkPredicted(Fragment, *Registry, LinkType, Hit.VictimId, Context.Frame))
				{
					continue;
				}
				// M7 G1: a type the initiator accepts by itself holds the type and the payload the target will form (same snapshot as the hit)
				const UGSTN_LinkTypeDefinition* Asked = Registry->FindDefinition(LinkType);
				const FGSTN_SnapshotEntry* Self = Context.Snapshot ? Context.Snapshot->Find(Context.EntityId) : nullptr;
				const FGSTN_SnapshotEntry* Victim = Context.Snapshot ? Context.Snapshot->Find(Hit.VictimId) : nullptr;
				if (Asked && Asked->bInitiatorPredicts && Self && Victim)
				{
					PredictRequest(Fragment.Get(Asked->Channel), *Registry, *Self, *Victim, LinkType, Context.Frame);
				}
			}
		}
		break;
	case EGSTN_CombatHookPoint::BeforeTagProjection:
	{
		const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		const bool bWasParked = IsParked(FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.In.Fragments), Registry);
		const bool bParked = IsParked(FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments), Registry);
		if (bParked && !bWasParked)
		{
			FGSTN_LinkedModifier::Queue(Context.Owner);
		}
		else if (bWasParked && !bParked)
		{
			FGSTN_LinkedModifier::Cancel(Context.Owner);
		}
		// The exit of a break (F11): the teleport to the watchdog's exit on the frame its command was applied, in the same Mover tick as the
		// parked mode's end
		if (const FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);
			Fragment && FGSTN_LinkExitRequest::IsDue(Fragment->Actions.Exit, Context.Frame))
		{
			FGSTN_LinkExitRequest::Apply(Context.AbilitySystem, Fragment->Actions.Exit);
		}
		// M7 G1: the link effects of roles that became Active or ended this frame, like the exit effect: from the fragment this frame wrote
		FGSTN_LinkEffects::Step(Context.AbilitySystem, FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.In.Fragments),
			FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments), Registry);
		// The pair's collision for this frame's Mover tick (F15): from the fragment this frame wrote, rewritten every simulated frame. An entity
		// that never linked has no fragment and nothing to undo
		if (Context.Owner)
		{
			const FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);
			const FGSTN_EntityRef Ignored = FGSTN_LinkExitCollision::GetIgnoredPartner(Fragment, Registry, Context.Frame);
			const UGSTN_CombatWorldSubsystem* World = Fragment ? UGSTN_CombatWorldSubsystem::Get(Context.Owner->GetWorld()) : nullptr;
			if (Fragment)
			{
				FGSTN_LinkExitCollision::Apply(Context.Owner, Ignored.IsNull() || Ignored.Kind != GSTNEntityKinds::Fighter || !World ? nullptr : World->FindOwnerActor(Ignored.Id));
			}
		}
		ProjectTags(Context);
		break;
	}
	default:
		break;
	}
}

bool FGSTN_LinkTickExtension::BlocksActions(const FGSTN_CombatTickContext& Context) const
{
	const FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);
	return Fragment && IsActionBlocked(*Fragment, UGSTN_LinkRegistry::Get());
}

void FGSTN_LinkTickExtension::ProjectTags(FGSTN_CombatTickContext& Context) const
{
	UAbilitySystemComponent* ASC = Context.AbilitySystem;
	if (!ASC)
	{
		return;
	}
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);

	FGameplayTagContainer Held;
	GatherStateTags(Fragment, Registry, Held);

	// The interact target of this frame, for an entity that is free to link (no link, nothing pending) and is in the snapshot
	if (Context.Snapshot && Context.EntityId != INDEX_NONE && (!Fragment || (Fragment->Links.Body.Phase == EGSTN_LinkPhase::None && Fragment->Links.Hand.Phase == EGSTN_LinkPhase::None)))
	{
		if (const FGSTN_SnapshotEntry* Self = Context.Snapshot->Find(Context.EntityId))
		{
			const FGameplayTag Target = ResolveInteractTarget(*Self, Context.Snapshot->Entries, Context.Frame, Context.StickOnPlane);
			if (Target.IsValid())
			{
				Held.AddTag(Target);
			}
		}
	}

	// Set, never added: nothing else writes these, and a re-simulated frame must not inherit what a predicted future left. Every meaning an
	// offer can project is in the set (the grab's always, so a kit without offers still clears it)
	FGameplayTagContainer Projectable;
	GatherProjectableTags(Registry, Projectable);
	Projectable.AddTag(GSTNLinkTags::Interact_Target_Grab);
	if (Registry)
	{
		for (const FGSTN_LinkOfferEntry& Offer : Registry->GetInteractOffers())
		{
			Projectable.AddTag(Offer.Offer.Meaning);
		}
	}
	for (const FGameplayTag& Tag : Projectable)
	{
		ASC->SetLooseGameplayTagCount(Tag, Held.HasTagExact(Tag) ? 1 : 0);
	}
}
