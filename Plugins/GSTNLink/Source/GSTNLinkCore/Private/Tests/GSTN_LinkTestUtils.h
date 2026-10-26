// Shared helpers of the link automation tests. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkTags.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNLinkTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A grab definition in the transient package: the target is parked on the initiator, as a grab is */
	inline UGSTN_LinkTypeDefinition* MakeGrabDefinition()
	{
		UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
		Definition->LinkType = GSTNLinkTags::Type_Grab.GetTag();
		Definition->Channel = EGSTN_LinkChannel::Body;
		Definition->InitiatorRole.RoleTag = GSTNLinkTags::Role_Grabber.GetTag();
		Definition->TargetRole.RoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
		Definition->TargetRole.bParked = true;
		return Definition;
	}

	/**
	 * A latch definition built from data alone: the initiator is parked on the enemy's socket and the enemy keeps acting. This is the
	 * "new link type with no C++" the acceptance line asks for; the test registers it by holding it, nothing else
	 */
	inline UGSTN_LinkTypeDefinition* MakeLatchDefinition()
	{
		UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
		Definition->LinkType = GSTNLinkTags::Type_Latch.GetTag();
		Definition->Channel = EGSTN_LinkChannel::Body;
		Definition->InitiatorRole.RoleTag = GSTNLinkTags::Role_Latcher.GetTag();
		Definition->InitiatorRole.bParked = true;
		Definition->InitiatorRole.bActsWhileLinked = true;
		Definition->InitiatorRole.SocketOnPartner = TEXT("back_socket");
		Definition->TargetRole.RoleTag = GSTNLinkTags::Role_Latched.GetTag();
		Definition->TargetRole.bActsWhileLinked = true;
		return Definition;
	}

	/** Whether any message contains the fragment */
	inline bool AnyContains(const TArray<FString>& Messages, const TCHAR* Fragment)
	{
		for (const FString& Message : Messages)
		{
			if (Message.Contains(Fragment))
			{
				return true;
			}
		}
		return false;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
