// Links, Network Prediction half.

#include "Sim/GSTN_LinkInboxKinds.h"

#include "Actions/GSTN_LinkActionProvider.h"

#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Sim/GSTN_LinkExitCollision.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "Sync/GSTN_InboxEntry.h"

bool FGSTN_LinkInboxHandlers::ApplyResponse(FGSTN_LinkFragment& Fragment, const FGSTN_LinkResponseEntry& Response, int32 SenderId)
{
	if (SenderId == INDEX_NONE || Response.LinkTypeIndex == 0)
	{
		return false;
	}
	// The response names the type the target FORMED, which may be an alternative of the one the initiator asked for (the grab's latch, F12b): the
	// channel is that type's definition's, and the initiator's own pending request to this target on it is this request whatever type it predicted.
	// Without the definition (a registry that does not know the index) either channel that carries the very type is searched, as before
	const FGSTN_EntityRef Target(GSTNEntityKinds::Fighter, static_cast<uint16>(SenderId));
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(Response.LinkTypeIndex) : nullptr;
	TArray<FGSTN_LinkState*, TInlineAllocator<2>> Channels;
	if (Definition)
	{
		Channels.Add(&Fragment.Get(Definition->Channel));
	}
	else
	{
		Channels = { &Fragment.Links.Body, &Fragment.Links.Hand };
	}
	for (FGSTN_LinkState* State : Channels)
	{
		// The initiator's own request to this target: still Pending, or already accepted optimistically (M7 G1: Active, role 1, flag Predicted)
		const bool bOptimistic = State->IsActive() && State->Role == 1 && State->HasFlag(GSTNLinkFlags::Predicted);
		const bool bOwnRequest = (State->Phase == EGSTN_LinkPhase::Pending || bOptimistic) && State->PartnerId == Target
			&& (Definition || State->LinkTypeIndex == Response.LinkTypeIndex);
		if (Response.bAccepted)
		{
			// Already applied: the same serial on role 1 (the re-arrival after a rollback of the sender). An optimistic accept is not applied
			// yet: the response below replaces it, which confirms it (the flag goes) or corrects a mispredicted serial or payload
			if (State->Role == 1 && State->Serial == Response.Serial && State->PartnerId == Target && !bOptimistic)
			{
				return false;
			}
			if (bOwnRequest || (State->Phase == EGSTN_LinkPhase::None && State->IsFormationNew(Response.Serial)))
			{
				FGSTN_LinkState Formed;
				Formed.LinkTypeIndex = Response.LinkTypeIndex;
				Formed.Role = 1;
				Formed.Phase = EGSTN_LinkPhase::Active;
				Formed.PartnerId = Target;
				Formed.Serial = Response.Serial;
				Formed.LinkFrame = Response.LinkFrame;
				Formed.Payload = Response.Payload;
				*State = Formed;
				return true;
			}
		}
		else if (bOwnRequest)
		{
			// Refused: the request whiffs, the serial stays
			State->Reset();
			return true;
		}
	}
	return false;
}

bool FGSTN_LinkInboxHandlers::ApplyCommand(FGSTN_LinkFragment& Fragment, const FGSTN_LinkCommandEntry& Command, int32 EntryFrame)
{
	FGSTN_LinkState& State = Fragment.Get(Command.Channel);
	if (Command.Kind == EGSTN_LinkCommandKind::Form)
	{
		if (State.Phase != EGSTN_LinkPhase::None || !State.IsFormationNew(Command.Serial) || Command.LinkTypeIndex == 0 || Command.Role == 0
			|| Command.PartnerId == INDEX_NONE)
		{
			return false;
		}
		FGSTN_LinkState Formed;
		Formed.LinkTypeIndex = Command.LinkTypeIndex;
		Formed.Role = Command.Role;
		Formed.Phase = EGSTN_LinkPhase::Active;
		Formed.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(Command.PartnerId));
		Formed.Serial = Command.Serial;
		Formed.LinkFrame = EntryFrame + 1;
		State = Formed;
		return true;
	}
	if (Command.Kind == EGSTN_LinkCommandKind::None || !State.MatchesBreak(Command.Serial))
	{
		return false;
	}
	FGSTN_LinkActions::FExit& Exit = Fragment.Actions.Exit;
	Exit.Frame = EntryFrame + 1;
	Exit.Position = FVector(Command.ExitPosition);
	Exit.YawDeg = Command.ExitYawDeg;
	Exit.Velocity = FVector(Command.ExitVelocity);
	Exit.Flags = Command.ExitFlags;
	if (Command.Kind == EGSTN_LinkCommandKind::Break)
	{
		const uint8 Reason = Command.Reason;
		// Both sides apply this command on the same frame: the window of the pair's collision starts there on both (F15)
		const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		FGSTN_LinkExitCollision::Begin(Fragment, State, Registry ? Registry->GetDefinition(State.LinkTypeIndex) : nullptr, Exit.Frame);
		State.Reset();
		State.BreakReason = Reason;
		// A partner move of this link ends with it (M7 G2): the outcome's break, an abort, or any other break of the link
		if (Command.Channel == EGSTN_LinkChannel::Body)
		{
			Fragment.Actions.Active = FGSTN_LinkActions::FActive();
			Fragment.Actions.PendingStart = FGSTN_LinkActions::FPendingStart();
		}
		if (Command.ImmunityEndFrame > 0)
		{
			Fragment.GrabImmunityEndFrame = FMath::Max(Fragment.GrabImmunityEndFrame, Command.ImmunityEndFrame);
		}
	}
	return true;
}

bool FGSTN_LinkInboxHandlers::ApplyPartnerMove(FGSTN_LinkFragment& Fragment, const FGSTN_LinkPartnerMoveEntry& Move, int32 EntryFrame)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	if (Move.Id <= 0 || !Body.IsActive() || Body.Role == 0 || Body.Serial != Move.Serial)
	{
		// Another link, or none: the move is not this link's (M7 spec 6.5 item 3: dropped, the initiator's part continues alone)
		return false;
	}
	FGSTN_LinkActions& Actions = Fragment.Actions;
	const int32 StartFrame = FMath::Max(Move.TargetFrame, EntryFrame + 1);
	if ((Actions.PendingStart.Id == Move.Id && Actions.PendingStart.Frame == StartFrame)
		|| (Actions.Active.Id == Move.Id && Actions.Active.Side == GSTNLinkActionSide::Partner && Actions.Active.StartFrame == StartFrame))
	{
		return false;
	}
	Actions.PendingStart.Frame = StartFrame;
	Actions.PendingStart.Id = Move.Id;
	return true;
}
void FGSTN_LinkInboxHandlers::HandleResponse(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
{
	const FGSTN_LinkResponseEntry* Response = Entry.Payload.GetPtr<FGSTN_LinkResponseEntry>();
	if (!Response || !Context.Sync)
	{
		return;
	}
	FGSTN_LinkFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(Context.Sync->Fragments);
	ApplyResponse(Fragment, *Response, Entry.SenderId);
}

void FGSTN_LinkInboxHandlers::HandleCommand(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
{
	const FGSTN_LinkCommandEntry* Command = Entry.Payload.GetPtr<FGSTN_LinkCommandEntry>();
	if (!Command || !Context.Sync)
	{
		return;
	}
	FGSTN_LinkFragment* Fragment = Command->Kind == EGSTN_LinkCommandKind::Form ? &FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(Context.Sync->Fragments)
		: FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Sync->Fragments);
	if (Fragment)
	{
		ApplyCommand(*Fragment, *Command, Entry.Frame);
	}
}

void FGSTN_LinkInboxHandlers::HandlePartnerMove(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
{
	const FGSTN_LinkPartnerMoveEntry* Move = Entry.Payload.GetPtr<FGSTN_LinkPartnerMoveEntry>();
	FGSTN_LinkFragment* Fragment = Move && Context.Sync ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Sync->Fragments) : nullptr;
	if (Fragment)
	{
		ApplyPartnerMove(*Fragment, *Move, Entry.Frame);
	}
}
bool FGSTN_LinkInboxHandlers::Register()
{
	FGSTN_InboxKindRegistry& Registry = FGSTN_InboxKindRegistry::Get();
	const bool bResponse = Registry.Register(GSTNLinkInboxKinds::Response, TEXT("Link.Response"), &FGSTN_LinkInboxHandlers::HandleResponse);
	const bool bCommand = Registry.Register(GSTNLinkInboxKinds::Command, TEXT("Link.Command"), &FGSTN_LinkInboxHandlers::HandleCommand);
	const bool bPartnerMove = Registry.Register(GSTNLinkInboxKinds::PartnerMove, TEXT("Link.PartnerMove"), &FGSTN_LinkInboxHandlers::HandlePartnerMove);
	return bResponse && bCommand && bPartnerMove;
}

void FGSTN_LinkInboxHandlers::Unregister()
{
	FGSTN_InboxKindRegistry::Get().Unregister(GSTNLinkInboxKinds::PartnerMove);
	FGSTN_InboxKindRegistry::Get().Unregister(GSTNLinkInboxKinds::Command);
	FGSTN_InboxKindRegistry::Get().Unregister(GSTNLinkInboxKinds::Response);
}

FGSTN_InboxEntry FGSTN_LinkInboxHandlers::MakeResponse(const FGSTN_LinkResponseEntry& Payload)
{
	FGSTN_InboxEntry Entry;
	Entry.Kind = GSTNLinkInboxKinds::Response;
	Entry.Payload = FInstancedStruct::Make(Payload);
	return Entry;
}

FGSTN_InboxEntry FGSTN_LinkInboxHandlers::MakeCommand(const FGSTN_LinkCommandEntry& Payload)
{
	FGSTN_InboxEntry Entry;
	Entry.Kind = GSTNLinkInboxKinds::Command;
	Entry.Payload = FInstancedStruct::Make(Payload);
	return Entry;
}

FGSTN_InboxEntry FGSTN_LinkInboxHandlers::MakePartnerMove(const FGSTN_LinkPartnerMoveEntry& Payload)
{
	FGSTN_InboxEntry Entry;
	Entry.Kind = GSTNLinkInboxKinds::PartnerMove;
	Entry.Payload = FInstancedStruct::Make(Payload);
	return Entry;
}