#include "Misc/AutomationTest.h"
#include "Abilities/GSTN_DefenceAbilities.h"
#include "Sim/GSTN_TumbleLanding.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GetupTravelTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	TArray<FGSTN_GetupTravelKey> MakeTravelKeys(std::initializer_list<TPair<int32, float>> Points)
	{
		TArray<FGSTN_GetupTravelKey> Out;
		for (const TPair<int32, float>& Point : Points)
		{
			FGSTN_GetupTravelKey Key;
			Key.Frame = Point.Key;
			Key.Fraction = Point.Value;
			Out.Add(Key);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGetupTravelSegmentsTest, "GSTNDefence.NP.GetupRoll.TravelSegments", GetupTravelTestFlags)
bool FGSTN_DefenceGetupTravelSegmentsTest::RunTest(const FString& Parameters)
{
	// The forward rock-up of roll-travel.md: still until 6, 70 % by 10, all by 16
	const TArray<FGSTN_GetupTravelKey> Roll = MakeTravelKeys({ { 0, 0.f }, { 6, 0.f }, { 10, 0.7f }, { 16, 1.f } });
	float Speed = 0.f;
	int32 Frames = 0;
	TestFalse(TEXT("frame 0: the hold has no push"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 0, Speed, Frames));
	TestTrue(TEXT("frame 6: the main push"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 6, Speed, Frames));
	TestEqual(TEXT("frame 6: four frames"), Frames, 4);
	TestEqual(TEXT("frame 6: 56 cm over the segment"), Speed * Frames * FGSTN_TumbleLanding::FrameSeconds, 56.f, 0.01f);
	TestTrue(TEXT("frame 10: the ease out"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 10, Speed, Frames));
	TestEqual(TEXT("frame 10: 24 cm over the segment"), Speed * Frames * FGSTN_TumbleLanding::FrameSeconds, 24.f, 0.01f);
	TestFalse(TEXT("frame 8: inside a segment, nothing starts"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 8, Speed, Frames));
	TestFalse(TEXT("frame 16: the last key starts nothing"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 16, Speed, Frames));
	TestFalse(TEXT("no distance: in place"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 0.f, 6, Speed, Frames));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
