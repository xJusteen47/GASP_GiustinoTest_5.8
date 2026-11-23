// Shared helpers of the defence automation tests. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_Curve1D.h"
#include "Data/GSTN_DefenceRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDefenceTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A rules asset in the transient package, at the data defaults */
	inline UGSTN_DefenceRules* MakeRules()
	{
		return NewObject<UGSTN_DefenceRules>(GetTransientPackage());
	}

	inline FGSTN_Curve1DPoint MakePoint(int32 X, int32 Y)
	{
		FGSTN_Curve1DPoint Point;
		Point.X = X;
		Point.Y = Y;
		return Point;
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
