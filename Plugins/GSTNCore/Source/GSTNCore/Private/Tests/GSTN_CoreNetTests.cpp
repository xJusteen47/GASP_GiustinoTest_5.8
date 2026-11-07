// Tests of the shared net primitives (no world). The GSTNPlatform.Core.PoseHistory.* tests stay in GSTNPlatform as the check that the
// move into GSTNCore changed nothing for the platform.

#include "Misc/AutomationTest.h"
#include "Net/GSTN_PoseHistory.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags CoreNetTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CorePoseHistoryResimTest, "GSTNCore.Net.PoseHistory.ResimOverwritesAndReset", CoreNetTestFlags)
bool FGSTN_CorePoseHistoryResimTest::RunTest(const FString& Parameters)
{
	FGSTN_PoseHistory History;
	History.Record(5, FTransform(FVector(100.0, 0.0, 0.0)), 0.016);
	History.Record(6, FTransform(FVector(200.0, 0.0, 0.0)), 0.016);

	// A re-simulation records frame 5 again: the slot is overwritten, the latest frame does not move back
	History.Record(5, FTransform(FVector(150.0, 0.0, 0.0)), 0.016);
	FGSTN_PoseSample Sample;
	TestTrue(TEXT("Frame 5 overwritten"), History.Sample(5, Sample) && Sample.Location.Equals(FVector(150.0, 0.0, 0.0)));
	TestEqual(TEXT("Latest frame stays 6"), History.GetLatestFrame(), 6);

	TestFalse(TEXT("Negative frames are never recorded"), History.Sample(-1, Sample));

	History.Reset();
	TestEqual(TEXT("Reset empties the ring"), History.GetLatestFrame(), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("No sample after reset"), History.Sample(6, Sample));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
