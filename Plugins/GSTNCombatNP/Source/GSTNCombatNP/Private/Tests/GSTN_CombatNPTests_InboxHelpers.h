// Shared helpers of the inbox automation tests: the hit tests drive FGSTN_CombatInbox with hit entries as they always did.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_HitInbox.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNInboxTestUtils
{
	/** Wraps a hit entry as kind Hit and adds it */
	inline bool AddHit(FGSTN_CombatInbox& Inbox, const FGSTN_HitInboxEntry& Entry)
	{
		return Inbox.Add(Entry.ToInboxEntry());
	}

	/** Drains Inbox at Frame and appends the hit entries of the result to Out, in the drain order */
	inline void DrainHits(FGSTN_CombatInbox& Inbox, int32 Frame, TArray<FGSTN_HitInboxEntry>& Out)
	{
		TArray<FGSTN_InboxEntry> Drained;
		Inbox.Drain(Frame, Drained);
		for (const FGSTN_InboxEntry& Entry : Drained)
		{
			if (const FGSTN_HitInboxEntry* Hit = FGSTN_HitInboxEntry::FromInboxEntry(Entry))
			{
				Out.Add(*Hit);
			}
		}
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
