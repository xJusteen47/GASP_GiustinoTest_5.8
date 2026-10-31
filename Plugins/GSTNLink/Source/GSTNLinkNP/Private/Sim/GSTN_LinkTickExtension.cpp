// Links, Network Prediction half.

#include "Sim/GSTN_LinkTickExtension.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_InteractMath.h"
#include "GSTN_InteractTags.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Movement/GSTN_LinkExitRequest.h"
#include "Movement/GSTN_LinkedModifier.h"
#include "Sim/GSTN_LinkAttach.h"
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

	/** The request side of a snapshot entry, as the LinkRequest step builds it from the same bytes */
	FGSTN_LinkRequestSide MakeSide(const FGSTN_SnapshotEntry& Entry, const FGSTN_LinkSnapshotExt* Ext)
	{
		FGSTN_LinkRequestSide Side;
		Side.EntityId = Entry.EntityId;
		Side.EffectiveWeight = 100.f * static_cast<float>(Entry.Reaction.WeightScaleQ) / 64.f;
		if (Ext)
		{
			FGSTN_LinkRequestTags::AppendTags(Ext->RequestTagMask, Side.Tags);
			Side.GrabImmunityEndFrame = Ext->GrabImmunityEndFrame;
			Side.CooldownEndFrame = Ext->CooldownEndFrame;
			Side.Links = Ext->ToLinkStates();
		}
		return Side;
	}
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

void FGSTN_LinkTickExtension::MakeGrabOffers(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkKitExtension* Kit, TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>>& OutOffers)
{
	const FGSTN_LinkInteractOffer Grab = FGSTN_LinkInteractProvider::MakeGrabOffer(Registry, Kit);
	if (!Grab.Definition)
	{
		return;
	}
	for (const FGameplayTag& Alternative : Grab.Definition->Alternatives)
	{
		if (const UGSTN_LinkTypeDefinition* Definition = Registry.FindDefinition(Alternative))
		{
			FGSTN_LinkInteractOffer Offer = Grab;
			Offer.Definition = Definition;
			OutOffers.Add(Offer);
		}
	}
	OutOffers.Add(Grab);
}

const FGSTN_InteractMeaningTable& FGSTN_LinkTickExtension::GetLinkMeanings()
{
	static FGSTN_InteractMeaningTable Meanings;
	static bool bBuilt = false;
	if (!bBuilt)
	{
		FGSTN_InteractMeaning Grab;
		Grab.Tag = GSTNLinkTags::Interact_Target_Grab;
		Meanings.Build({ Grab });
		bBuilt = true;
	}
	return Meanings;
}

FGameplayTag FGSTN_LinkTickExtension::ResolveGrabTarget(const FGSTN_SnapshotEntry& Self, TConstArrayView<FGSTN_SnapshotEntry> Entries, int32 Frame,
	const FVector& StickOnPlane, const FGSTN_InteractMeaningTable& Meanings) const
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkSnapshotExt* SelfExt = Self.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	if (!Registry || !SelfExt || !SelfExt->IsValid())
	{
		return FGameplayTag();
	}
	// Only a kit that can grab (it carries a link extension) looks for a grab target: the probe reads other entities' snapshot positions, which
	// a client knows as interpolated proxies, so its result is not prediction-stable and must not reach a pawn that never grabs (F10 deviation)
	const FGSTN_LinkBodyData* Body = FGSTN_LinkAttach::At(SelfExt->BodyIndex);
	if (!Body || !Body->GetKitExtension())
	{
		return FGameplayTag();
	}
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>> Offers;
	MakeGrabOffers(*Registry, Body->GetKitExtension(), Offers);
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
		Candidate.Side = MakeSide(Entry, Entry.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link));
		Candidate.Location = Entry.Transform.GetLocation();
		Candidate.RadiusCm = FMath::RoundToInt32(FMath::Min(Entry.BoundRadius, 60.f));
	}
	if (Candidates.IsEmpty())
	{
		return FGameplayTag();
	}

	FGSTN_LinkInteractSource Source;
	Source.Interactor = MakeSide(Self, SelfExt);
	Source.Entities = Candidates;
	Source.Offers = Offers;
	Source.Registry = Registry;

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
	return Projected.HasTagExact(GSTNLinkTags::Interact_Target_Grab) ? GSTNLinkTags::Interact_Target_Grab.GetTag() : FGameplayTag();
}

void FGSTN_LinkTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments))
		{
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
				MarkPredicted(Fragment, *Registry, LinkType, Hit.VictimId, Context.Frame);
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

	// The grab target of this frame, for an entity that is free to grab (no link, nothing pending) and is in the snapshot
	if (Context.Snapshot && Context.EntityId != INDEX_NONE && (!Fragment || (Fragment->Links.Body.Phase == EGSTN_LinkPhase::None && Fragment->Links.Hand.Phase == EGSTN_LinkPhase::None)))
	{
		if (const FGSTN_SnapshotEntry* Self = Context.Snapshot->Find(Context.EntityId))
		{
			const FGameplayTag Target = ResolveGrabTarget(*Self, Context.Snapshot->Entries, Context.Frame, Context.StickOnPlane, GetLinkMeanings());
			if (Target.IsValid())
			{
				Held.AddTag(Target);
			}
		}
	}

	// Set, never added: nothing else writes these, and a re-simulated frame must not inherit what a predicted future left
	FGameplayTagContainer Projectable;
	GatherProjectableTags(Registry, Projectable);
	Projectable.AddTag(GSTNLinkTags::Interact_Target_Grab);
	for (const FGameplayTag& Tag : Projectable)
	{
		ASC->SetLooseGameplayTagCount(Tag, Held.HasTagExact(Tag) ? 1 : 0);
	}
}
