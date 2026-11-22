// Tests of the shared net primitives (no world). The GSTNPlatform.Core.PoseHistory.* tests stay in GSTNPlatform as the check that the
// move into GSTNCore changed nothing for the platform.

#include "Misc/AutomationTest.h"
#include "Net/GSTN_HostSuitability.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreHostSuitabilityTest, "GSTNCore.Net.HostSuitability.ScoreOrder", CoreNetTestFlags)
bool FGSTN_CoreHostSuitabilityTest::RunTest(const FString& Parameters)
{
	FGSTN_HostSuitability Strong;
	Strong.PhysicalCores = 8;
	Strong.BenchmarkMs = FGSTN_HostSuitability::ReferenceBenchmarkMs * 0.5;
	FGSTN_HostSuitability::Evaluate(Strong);
	FGSTN_HostSuitability Toaster;
	Toaster.PhysicalCores = 2;
	Toaster.BenchmarkMs = FGSTN_HostSuitability::ReferenceBenchmarkMs * 3.0;
	FGSTN_HostSuitability::Evaluate(Toaster);
	TestTrue(TEXT("Faster, wider machine scores higher"), Strong.Score > Toaster.Score);
	TestTrue(TEXT("Strong is eligible, 2 cores is not"), Strong.bEligible && !Toaster.bEligible);

	FGSTN_HostSuitability Thin = Strong;
	Thin.UploadKBps = FGSTN_HostSuitability::RequiredUploadKBps * 0.5;
	FGSTN_HostSuitability::Evaluate(Thin);
	TestTrue(TEXT("Low known upload halves the score and is not eligible"), FMath::IsNearlyEqual(Thin.Score, Strong.Score * 0.5) && !Thin.bEligible);

	const FGSTN_HostSuitability Here = FGSTN_HostSuitability::Measure();
	TestTrue(TEXT("This machine measures a positive benchmark and score"), Here.BenchmarkMs > 0.0 && Here.Score > 0.0 && Here.LogicalCores > 0);
	AddInfo(Here.ToString());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
