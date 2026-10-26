// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_HitInbox.h"

#include "Simulation/GSTN_CombatInbox.h"

FGSTN_InboxEntry FGSTN_HitInboxEntry::ToInboxEntry() const
{
	FGSTN_InboxEntry Entry;
	Entry.Kind = GSTNInboxKinds::Hit;
	Entry.Frame = Frame;
	Entry.SenderId = AttackerId;
	// Make<Derived> and not the base: the payload keeps the type the unwrap asks for
	Entry.Payload = FInstancedStruct::Make<FGSTN_HitInboxEntry>(*this);
	return Entry;
}

const FGSTN_HitInboxEntry* FGSTN_HitInboxEntry::FromInboxEntry(const FGSTN_InboxEntry& Entry)
{
	return Entry.Kind == GSTNInboxKinds::Hit ? Entry.Payload.GetPtr<FGSTN_HitInboxEntry>() : nullptr;
}
