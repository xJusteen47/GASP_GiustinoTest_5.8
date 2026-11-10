// Held objects, simulation side (needs the GAS-NPP fork).

#include "GSTN_HeldSim.h"

#include "AbilitySystemComponent.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_HeldTags.h"
#include "World/GSTN_StepSnapshot.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_CombatSimTypes.h"

void FGSTN_HeldFragment::NetSerialize(FArchive& Ar)
{
	Ar << ObjectRef << Serial << HoldFrame;
}

bool FGSTN_HeldFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	return !(*this == static_cast<const FGSTN_HeldFragment&>(Authority));
}

void FGSTN_HeldFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	if (IsHolding())
	{
		Out.Appendf("Held obj %d #%d@%d", static_cast<int32>(ObjectRef) - 1, static_cast<int32>(Serial), HoldFrame);
	}
	else
	{
		Out.Appendf("Held empty #%d", static_cast<int32>(Serial));
	}
}

bool FGSTN_HeldInbox::Register()
{
	const bool bFragment = FGSTN_SyncFragmentRegistry::Register<FGSTN_HeldFragment>(GSTNFeatureIds::Held);
	const bool bKind = FGSTN_InboxKindRegistry::Get().Register(GSTNHeldInboxKinds::Command, TEXT("Held.Command"),
		[](const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
		{
			const FGSTN_HeldCommandEntry* Command = Entry.Payload.GetPtr<FGSTN_HeldCommandEntry>();
			if (!Command || !Context.Sync)
			{
				return;
			}
			// A release of a hand that never held anything adds nothing
			FGSTN_HeldFragment* Fragment = Command->bHold ? &FGSTN_SyncFragmentList::FindOrAdd<FGSTN_HeldFragment>(Context.Sync->Fragments)
				: FGSTN_SyncFragmentList::Find<FGSTN_HeldFragment>(Context.Sync->Fragments);
			if (Fragment)
			{
				Apply(*Fragment, *Command, Context.Frame);
			}
		});
	return bFragment && bKind;
}

void FGSTN_HeldInbox::Unregister()
{
	FGSTN_InboxKindRegistry::Get().Unregister(GSTNHeldInboxKinds::Command);
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::Held);
}

FGSTN_InboxEntry FGSTN_HeldInbox::MakeCommand(const FGSTN_HeldCommandEntry& Payload)
{
	FGSTN_InboxEntry Entry;
	Entry.Kind = GSTNHeldInboxKinds::Command;
	Entry.Payload = FInstancedStruct::Make(Payload);
	return Entry;
}

void FGSTN_HeldInbox::Apply(FGSTN_HeldFragment& Fragment, const FGSTN_HeldCommandEntry& Command, int32 Frame)
{
	if (Command.bHold)
	{
		Fragment.ObjectRef = Command.ObjectRef;
		Fragment.Serial = Command.Serial;
		Fragment.HoldFrame = Frame;
		return;
	}
	if (Fragment.ObjectRef == Command.ObjectRef && Fragment.Serial == Command.Serial)
	{
		Fragment.ObjectRef = 0;
		Fragment.HoldFrame = INDEX_NONE;
	}
}

void FGSTN_HeldTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	if (Point != EGSTN_CombatHookPoint::BeforeTagProjection || !Context.AbilitySystem)
	{
		return;
	}
	const FGSTN_HeldFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_HeldFragment>(Context.Out.Fragments);
	const bool bHolding = Fragment && Fragment->IsHolding();
	Context.AbilitySystem->SetLooseGameplayTagCount(GSTNHeldTags::Held_Carrying, bHolding ? 1 : 0);
	// Interact.Target.Pickup is not projected here since M10 G2: an object at rest is a stamped GSTNInteract record and GSTNLink's interact
	// probe projects its meaning, the one Interact.Target tag of the frame (UGSTN_HeldObjectComponent::ApplyRecord)
}
