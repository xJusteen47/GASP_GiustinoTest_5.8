// greybox-capsula 1A: the start measure of the speed graph (t50 / t90).
#include "DebugDraw/GSTN_SpeedGraph.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugSpeedGraphMeasureTest, "GSTNCombat.Debug.SpeedGraph.MeasureStart",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugSpeedGraphMeasureTest::RunTest(const FString& Parameters)
{
	// Linear ramp to 600 cm/s in 1 s, then flat
	TArray<FVector2D> Ramp;
	for (int32 Frame = 0; Frame <= 90; ++Frame)
	{
		const double T = Frame / 60.0;
		Ramp.Emplace(T, FMath::Min(T, 1.0) * 600.0);
	}
	float T50 = 0.f;
	float T90 = 0.f;
	TestTrue(TEXT("a full ramp is measured"), FGSTN_SpeedGraph::MeasureStart(Ramp, T50, T90));
	TestEqual(TEXT("t50 at half the ramp"), T50, 0.5f, 1.f / 60.f);
	TestEqual(TEXT("t90 at 90 % of the ramp"), T90, 0.9f, 1.f / 60.f);

	TArray<FVector2D> Still;
	Still.Emplace(0.0, 0.0);
	Still.Emplace(0.1, 5.0);
	TestFalse(TEXT("no start under the rest speed"), FGSTN_SpeedGraph::MeasureStart(Still, T50, T90));
	return true;
}

#endif
