// Links, Network Prediction half.

#include "Sim/GSTN_LinkTickExtension.h"

#include "AbilitySystemComponent.h"
#include "Actions/GSTN_LinkActionProvider.h"
#include "Components/CapsuleComponent.h"
#include "GSTN_LinkWatchdog.h"
#include "Events/GSTN_SimEvent.h"
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
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "State/GSTN_LinkMath.h"
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

// ---- Partner moves (M7 G2)

namespace
{
	/** Ends a partner move on a failure path: the record goes (so the watchdog uses the type's default exit) and the link breaks with Forced */
	void AbortPartnerMove(FGSTN_LinkFragment& Fragment, int16 Id, FGSTN_LinkTickExtension::FPartnerMoveResult& OutResult)
	{
		Fragment.Actions.Active = FGSTN_LinkActions::FActive();
		Fragment.Actions.PendingStart = FGSTN_LinkActions::FPendingStart();
		FGSTN_LinkTickExtension::MarkPendingBreak(Fragment, static_cast<uint8>(EGSTN_LinkBreakReason::Forced));
		OutResult.AbortedId = Id;
	}

	uint8 ActionBreakReason(const FGSTN_LinkActionInfo& Info, const UGSTN_LinkRegistry* Registry)
	{
		const uint8 Index = Info.BreakReason.IsValid() && Registry ? Registry->GetReasons().IndexOf(Info.BreakReason) : 0;
		return Index != 0 ? Index : static_cast<uint8>(EGSTN_LinkBreakReason::Release);
	}
}

bool FGSTN_LinkTickExtension::StartPartnerMove(FGSTN_LinkFragment& Fragment, int16 Id, int32 Frame, const FGSTN_LinkActionInfo& Info, FGSTN_LinkPartnerMoveEntry& OutEntry)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	FGSTN_LinkActions& Actions = Fragment.Actions;
	if (Id <= 0 || !Body.IsActive() || Body.Role == 0 || Body.PartnerId.IsNull() || Body.HasFlag(GSTNLinkFlags::PendingBreak)
		|| Actions.Active.Id != 0 || Actions.PendingStart.Id != 0)
	{
		return false;
	}
	Actions.Active.Id = Id;
	Actions.Active.StartFrame = Frame;
	Actions.Active.Side = GSTNLinkActionSide::Initiator;
	OutEntry.Serial = Body.Serial;
	OutEntry.Id = Id;
	// The entry reaches the partner the frame after it is posted at the earliest (the inbox rule), so an offset below 1 starts it there
	OutEntry.TargetFrame = Frame + FMath::Max(Info.PartnerStartOffsetFrames, 1);
	return true;
}

bool FGSTN_LinkTickExtension::StepPartnerMoveKeys(const FGSTN_CombatTickContext& Context, FGSTN_LinkFragment& Fragment, TArray<FPartnerMovePost, TInlineAllocator<1>>& OutPosts)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Context.RunningTimeline || !Registry || !Body.IsActive() || Body.Role == 0 || Body.PartnerId.IsNull())
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
	bool bChanged = false;
	for (const FGSTN_LinkOutcomeKey* Key : Keys)
	{
		if (Key->Outcome != EGSTN_LinkOutcome::PartnerMove || Key->PartnerMoveId <= 0 || Key->PartnerMoveId > MAX_int16)
		{
			continue;
		}
		const int16 Id = static_cast<int16>(Key->PartnerMoveId);
		FGSTN_LinkActionInfo Info;
		FGSTN_LinkPartnerMoveEntry Entry;
		if (Registry->ResolveLinkAction(Body.LinkTypeIndex, Id, Info) && StartPartnerMove(Fragment, Id, Context.Frame, Info, Entry))
		{
			FPartnerMovePost& Post = OutPosts.AddDefaulted_GetRef();
			Post.PartnerId = static_cast<int32>(Body.PartnerId.Id);
			Post.Entry = Entry;
			bChanged = true;
		}
	}
	return bChanged;
}

bool FGSTN_LinkTickExtension::StepPartnerMoves(FGSTN_LinkFragment& Fragment, const FPartnerMoveFrame& In, const UGSTN_LinkRegistry* Registry, FPartnerMoveResult& OutResult)
{
	FGSTN_LinkActions& Actions = Fragment.Actions;
	if (Actions.Active.Id == 0 && Actions.PendingStart.Id == 0)
	{
		return false;
	}
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Body.IsActive() || Body.Role == 0)
	{
		// The link ended without a command that cleared the record (a reset by another rule): the move ends with it
		Actions.Active = FGSTN_LinkActions::FActive();
		Actions.PendingStart = FGSTN_LinkActions::FPendingStart();
		return true;
	}

	bool bChanged = false;
	// The partner's scheduled part, on its frame (the handler never schedules it before the frame after the post)
	if (Actions.PendingStart.Id != 0 && In.Frame >= Actions.PendingStart.Frame)
	{
		const int16 Id = Actions.PendingStart.Id;
		Actions.PendingStart = FGSTN_LinkActions::FPendingStart();
		bChanged = true;
		FGSTN_LinkActionInfo Info;
		if (In.bKnockedOut)
		{
			AbortPartnerMove(Fragment, Id, OutResult);
			return true;
		}
		if (Actions.Active.Id != 0 || !In.bCanStart || !Registry || !Registry->ResolveLinkAction(Body.LinkTypeIndex, Id, Info)
			|| !Info.PartnerRequirements.IsSatisfiedBy(In.OwnTags))
		{
			// The part cannot play (already in a move, in a hit reaction, unknown, requirements): dropped, the link kept (M7 spec 6.5 item 3)
			OutResult.AbortedId = Id;
			return true;
		}
		Actions.Active.Id = Id;
		Actions.Active.StartFrame = In.Frame;
		Actions.Active.Side = GSTNLinkActionSide::Partner;
		OutResult.StartAbility = Info.PartnerAbility;
		return true;
	}
	if (Actions.Active.Id == 0)
	{
		return bChanged;
	}

	const int16 Id = Actions.Active.Id;
	FGSTN_LinkActionInfo Info;
	if (!Registry || !Registry->ResolveLinkAction(Body.LinkTypeIndex, Id, Info))
	{
		Actions.Active = FGSTN_LinkActions::FActive();
		return true;
	}
	if (In.bKnockedOut || (Info.MaxDurationFrames > 0 && In.Frame >= Actions.Active.StartFrame + Info.MaxDurationFrames))
	{
		AbortPartnerMove(Fragment, Id, OutResult);
		return true;
	}

	if (Actions.Active.Side == GSTNLinkActionSide::Initiator)
	{
		if (Body.HasFlag(GSTNLinkFlags::PendingBreak))
		{
			// The outcome's break waits for the watchdog's command: the record stays, so the break leaves through the action's exit
			return bChanged;
		}
		const bool bSameRun = In.bInTimeline && In.TimelineStartFrame != INDEX_NONE && In.TimelineStartFrame <= Actions.Active.StartFrame;
		if (!bSameRun)
		{
			// The initiator's move was replaced or interrupted before its outcome: its record ends, the partner's part plays on alone
			Actions.Active = FGSTN_LinkActions::FActive();
			return true;
		}
		if (Info.OutcomeFrame != INDEX_NONE && In.bTimelineCrossed && In.TimelinePrevious < Info.OutcomeFrame && Info.OutcomeFrame <= In.TimelineCurrent)
		{
			switch (Info.Outcome)
			{
			case EGSTN_LinkActionOutcome::Break:
				MarkPendingBreak(Fragment, ActionBreakReason(Info, Registry));
				break;
			case EGSTN_LinkActionOutcome::SwapRoles:
			{
				static bool bWarned = false;
				UE_CLOG(!bWarned, LogTemp, Warning, TEXT("A partner move asks for SwapRoles, which is not built (M7 G2): treated as KeepLink"));
				bWarned = true;
				Actions.Active = FGSTN_LinkActions::FActive();
				break;
			}
			default:
				Actions.Active = FGSTN_LinkActions::FActive();
				break;
			}
			return true;
		}
	}
	else if (Actions.Active.Side == GSTNLinkActionSide::Partner)
	{
		// The forced part started on Active.StartFrame; once its timeline is not that run any more, the part is over and the lock lifts
		if (In.Frame > Actions.Active.StartFrame && (!In.bInTimeline || In.TimelineStartFrame != Actions.Active.StartFrame))
		{
			Actions.Active = FGSTN_LinkActions::FActive();
			return true;
		}
	}
	return bChanged;
}

bool FGSTN_LinkTickExtension::IsCommandLocked(const FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry)
{
	const FGSTN_LinkActions::FActive& Active = Fragment.Actions.Active;
	if (Active.Id == 0 || Active.Side != GSTNLinkActionSide::Partner || !Registry)
	{
		return false;
	}
	FGSTN_LinkActionInfo Info;
	return Registry->ResolveLinkAction(Fragment.Links.Body.LinkTypeIndex, Active.Id, Info) && Info.bPartnerLocksCommands;
}

// ---- Parked facing (M7 G2)

bool FGSTN_LinkTickExtension::StepFacing(FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry, const FTransform& Anchor, const FVector& StickOnPlane,
	bool bIdle, bool bMoveStarted)
{
	FGSTN_LinkState& Body = Fragment.Links.Body;
	// Below a quarter tilt the stick names no direction: the turn stays (a role that never pushes the stick keeps the placement's facing)
	constexpr double MinStick = 0.25;
	if (!Registry || !Body.IsActive() || Body.Role == 0 || !(bIdle || bMoveStarted) || StickOnPlane.SizeSquared() < MinStick * MinStick)
	{
		return false;
	}
	const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(Body.LinkTypeIndex);
	if (!Definition || Definition->Channel != EGSTN_LinkChannel::Body || Definition->GetParkedRoleIndex() != Body.Role)
	{
		return false;
	}
	const FGSTN_LinkRoleDef& Role = Definition->GetRoleDef(Body.Role);
	if (!Role.bActsWhileLinked || Role.YawLimitDeg <= 0.f)
	{
		return false;
	}
	const int16 Turn = UGSTN_LinkMath::FacingYawQ(Anchor, StickOnPlane, Role.YawLimitDeg, Body.LocalYawQ);
	if (Turn == Body.LocalYawQ)
	{
		return false;
	}
	Body.LocalYawQ = Turn;
	return true;
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

bool FGSTN_LinkTickExtension::StepInitiatorExit(FGSTN_LinkFragment& Fragment, int32 Frame, const UGSTN_LinkRegistry* Registry, const FGSTN_SnapshotEntry* PartnerInView,
	const FExitSelf& Self)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	const UGSTN_LinkTypeDefinition* Definition = BodyDefinition(Fragment, Registry);
	if (!Definition || !Definition->bInitiatorPlacesExit || !Body.IsActive() || Body.Role != 1 || Definition->GetParkedRoleIndex() != 1
		|| !Body.HasFlag(GSTNLinkFlags::PendingBreak) || Fragment.Actions.Exit.Frame != 0 || !PartnerInView
		|| Body.PartnerId.IsNull() || static_cast<int32>(Body.PartnerId.Id) != PartnerInView->EntityId)
	{
		return false;
	}
	// The placer the watchdog would pick: the playing partner move's exit, else the type's own
	const UGSTN_LinkExitPlacer* MovePlacer = nullptr;
	if (Fragment.Actions.Active.Id != 0 && Registry)
	{
		FGSTN_LinkActionInfo Info;
		MovePlacer = Registry->ResolveLinkAction(Body.LinkTypeIndex, Fragment.Actions.Active.Id, Info) ? Info.ExitPlacer : nullptr;
	}
	FGSTN_LinkWatchdogEntity Exiting;
	Exiting.Up = Self.Up;
	Exiting.CapsuleRadius = Self.CapsuleRadius;
	Exiting.CapsuleHalfHeight = Self.CapsuleHalfHeight;
	FGSTN_LinkWatchdogEntity Staying;
	Staying.Transform = PartnerInView->Transform;
	Staying.Transform.SetScale3D(FVector::OneVector);
	Staying.Up = PartnerInView->Up;
	Staying.Velocity = PartnerInView->Velocity;
	// No collision: the world is not rollback-safe state. The watchdog checks the place with the world before it copies it
	const FGSTN_ExitResult Exit = FGSTN_LinkWatchdogLogic::PlaceExit(*Definition, Exiting, Staying, nullptr, MovePlacer);
	FGSTN_LinkActions::FExit& Record = Fragment.Actions.Exit;
	Record.Frame = Frame + 1;
	Record.Position = Exit.Transform.GetLocation();
	Record.YawDeg = static_cast<float>(Exit.Transform.Rotator().Yaw);
	Record.Velocity = Exit.Velocity;
	Record.Flags = GSTNLinkExitFlags::Teleport | GSTNLinkExitFlags::InitiatorPlaced;
	return true;
}

bool FGSTN_LinkTickExtension::StepOwnExitBreak(FGSTN_LinkFragment& Fragment, int32 Frame, const UGSTN_LinkRegistry* Registry)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	const FGSTN_LinkActions::FExit& Exit = Fragment.Actions.Exit;
	const UGSTN_LinkTypeDefinition* Definition = BodyDefinition(Fragment, Registry);
	if (!Definition || !Definition->bInitiatorPlacesExit || !Body.IsLinked() || Body.Role != 1 || !Body.HasFlag(GSTNLinkFlags::PendingBreak)
		|| Exit.Frame != Frame || (Exit.Flags & GSTNLinkExitFlags::InitiatorPlaced) == 0)
	{
		return false;
	}
	// The watchdog's command for this side, as it would carry the copied exit (the parked initiator gets no grab immunity: that is role 2's)
	FGSTN_LinkCommandEntry Command;
	Command.Kind = EGSTN_LinkCommandKind::Break;
	Command.Channel = EGSTN_LinkChannel::Body;
	Command.Serial = Body.Serial;
	Command.Reason = Body.BreakReason != 0 ? Body.BreakReason : static_cast<uint8>(EGSTN_LinkBreakReason::Forced);
	Command.ExitPosition = FVector3f(Exit.Position);
	Command.ExitYawDeg = Exit.YawDeg;
	Command.ExitVelocity = FVector3f(Exit.Velocity);
	Command.ExitFlags = Exit.Flags;
	return FGSTN_LinkInboxHandlers::ApplyCommand(Fragment, Command, Frame - 1);
}

void FGSTN_LinkTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments))
		{
			// M7 G6: an initiator-placed exit due this frame ends the link here, as the command drained before this point did on the authority
			StepOwnExitBreak(*Fragment, Context.Frame, UGSTN_LinkRegistry::Get());
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
			// M7 G2: partner moves. The keys of the running move post the partner's part (stamped this frame, drained at the partner's next; a partner
			// that does not tick on this machine refuses it, the authority always takes it), then the scheduled part, the outcome and the failure paths
			TArray<FPartnerMovePost, TInlineAllocator<1>> PartnerMoves;
			StepPartnerMoveKeys(Context, *Fragment, PartnerMoves);
			if (!PartnerMoves.IsEmpty() && Context.Owner)
			{
				if (UGSTN_CombatWorldSubsystem* World = UGSTN_CombatWorldSubsystem::Get(Context.Owner->GetWorld()))
				{
					for (const FPartnerMovePost& Post : PartnerMoves)
					{
						FGSTN_InboxEntry Entry = FGSTN_LinkInboxHandlers::MakePartnerMove(Post.Entry);
						Entry.Frame = Context.Frame;
						Entry.SenderId = Context.EntityId;
						World->PostToInbox(Post.PartnerId, Entry);
					}
				}
			}
			if (Fragment->Actions.Active.Id != 0 || Fragment->Actions.PendingStart.Id != 0)
			{
				FPartnerMoveFrame MoveFrame;
				MoveFrame.Frame = Context.Frame;
				// The partner requirements read the entity's own snapshot entry (end of the previous frame), as a link rule reads a request side
				const FGSTN_SnapshotEntry* Self = Context.Snapshot && Context.EntityId != INDEX_NONE ? Context.Snapshot->Find(Context.EntityId) : nullptr;
				if (Self)
				{
					MoveFrame.OwnTags = MakeRequestSide(*Self).Tags;
				}
				const FGSTN_CombatSyncCore& Core = Context.Out.Core;
				MoveFrame.bKnockedOut = Core.ActionState == EGSTN_ActionState::KO;
				MoveFrame.bCanStart = Core.ActionState == EGSTN_ActionState::Actionable || Core.ActionState == EGSTN_ActionState::InTimeline;
				MoveFrame.bInTimeline = Core.ActionState == EGSTN_ActionState::InTimeline;
				MoveFrame.TimelineStartFrame = Core.TimelineStartFrame;
				MoveFrame.bTimelineCrossed = FGSTN_LinkOutcomeKeys::GetCrossedFrames(Context.In.Core, Core, Context.Frame, MoveFrame.TimelinePrevious, MoveFrame.TimelineCurrent);
				FPartnerMoveResult MoveResult;
				StepPartnerMoves(*Fragment, MoveFrame, Registry, MoveResult);
				if (MoveResult.StartAbility && !(Context.Services && Context.Services->StartForcedTimeline(MoveResult.StartAbility)))
				{
					// The partner's kit has no such timeline ability: the part is dropped as one whose requirements fail
					MoveResult.AbortedId = Fragment->Actions.Active.Id;
					Fragment->Actions.Active = FGSTN_LinkActions::FActive();
				}
				if (MoveResult.AbortedId != 0 && Context.Outbox)
				{
					FGSTN_SimEvent Event;
					Event.EventType = GSTNLinkTags::Event_Link_PartnerMoveAborted;
					Event.Frame = Context.Frame;
					Event.Instigator = Context.Owner;
					Event.Magnitude = static_cast<float>(MoveResult.AbortedId);
					Context.Outbox->Add(MoveTemp(Event));
				}
			}
			// M7 G6: a parked initiator that releases (any PendingBreak of this frame or before) places its own exit from its view: on the authority,
			// for a remote player, the partner as that player saw it. Asked only then, so the view costs nothing on any other frame
			if (Fragment->Links.Body.HasFlag(GSTNLinkFlags::PendingBreak) && Fragment->Actions.Exit.Frame == 0 && Fragment->Links.Body.Role == 1)
			{
				const FGSTN_StepSnapshot* View = Context.Services ? Context.Services->GetViewSnapshot() : nullptr;
				View = View ? View : Context.Snapshot;
				const FGSTN_LinkState& Body = Fragment->Links.Body;
				const FGSTN_SnapshotEntry* Partner = View && !Body.PartnerId.IsNull() ? View->Find(Body.PartnerId.Id) : nullptr;
				FExitSelf Self;
				Self.Up = Context.Up;
				if (const UCapsuleComponent* Capsule = Context.Owner ? Cast<UCapsuleComponent>(Context.Owner->GetRootComponent()) : nullptr)
				{
					// The capsule's size is content (fixed for the pawn), not state: the watchdog reads the same numbers
					Self.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
					Self.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
				}
				StepInitiatorExit(*Fragment, Context.Frame, Registry, Partner, Self);
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
		// M7 G2: the turn of a parked role that acts, after step 8 (a move that started this frame is known), relative to its placement in the
		// step snapshot (the partner at the end of the previous frame, the transform the hurtboxes of this frame use)
		if (FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);
			Fragment && Fragment->Links.Body.IsActive() && Context.Snapshot && Context.EntityId != INDEX_NONE)
		{
			const FGSTN_SnapshotEntry* Self = Context.Snapshot->Find(Context.EntityId);
			const FGSTN_LinkSnapshotExt* SelfExt = Self ? Self->Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link) : nullptr;
			const FGSTN_SnapshotEntry* Partner = SelfExt && SelfExt->IsValid() && SelfExt->IsParked() ? Context.Snapshot->Find(SelfExt->PartnerId) : nullptr;
			FTransform Anchor;
			if (Partner && FGSTN_LinkSnapshotContributor::ComputeParkedTransform(*SelfExt, *Partner, Anchor))
			{
				const FGSTN_CombatSyncCore& Out = Context.Out.Core;
				const FGSTN_CombatSyncCore& In = Context.In.Core;
				const bool bIdle = Out.ActionState == EGSTN_ActionState::Actionable;
				const bool bMoveStarted = Out.ActionState == EGSTN_ActionState::InTimeline && (In.ActionState != EGSTN_ActionState::InTimeline
					|| In.ActiveTimeline != Out.ActiveTimeline || In.TimelineStartFrame != Out.TimelineStartFrame);
				StepFacing(*Fragment, UGSTN_LinkRegistry::Get(), Anchor, Context.StickOnPlane, bIdle, bMoveStarted);
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
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	return Fragment && (IsActionBlocked(*Fragment, Registry) || IsCommandLocked(*Fragment, Registry));
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
