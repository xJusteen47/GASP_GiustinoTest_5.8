// Shared helpers of the minion Core automation tests. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNMinionTest
{
	/** Same flags as the other GSTN plugins: runs in every application context, part of the product filter */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

#endif // WITH_DEV_AUTOMATION_TESTS
