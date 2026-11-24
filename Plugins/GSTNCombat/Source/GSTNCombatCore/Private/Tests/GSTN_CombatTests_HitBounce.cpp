// Hit Bounce track (owner 2026-11-23, the Cr.L Aerial stomp): FGSTN_HitBounceRules, pure.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_HitBounce.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNHitBounceTests
{
	constexpr double Step = 0.016;

	/** Integrates a launch the way Falling does (velocity, then position, one fixed step at a time) with the bounce's gravity; the height at each frame */
	TArray<double> Heights(const FGSTN_HitBounceLaunch& Launch, double ModeGravity, int32 Frames)
	{
		TArray<double> Out;
		double Height = 0.0;
		double Up = Launch.Velocity.Z;
		const double Gravity = ModeGravity * Launch.GravityScale;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Height += Up * Step - 0.5 * Gravity * Step * Step;
			Up -= Gravity * Step;
			Out.Add(Height);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceResolveTest, "GSTNCombat.Core.HitBounce.Resolve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitBounceResolveTest::RunTest(const FString& Parameters)
{
	FGSTN_TimelineTrack_HitBounce Track;
	FGSTN_HitBounceOverrides None;
	const FGSTN_HitBounceTuning Data = FGSTN_HitBounceRules::Resolve(Track, None);
	TestEqual(TEXT("owner default: 2 bounces in a row"), Data.MaxConsecutiveBounces, 2);
	TestEqual(TEXT("owner default: apex 150 cm"), Data.ApexHeightCm, 150.f);
	TestEqual(TEXT("owner default: apex in 18 frames"), Data.RiseFrames, 18);
	TestEqual(TEXT("owner default: free after 24 frames"), Data.ActionableFrames, 24);
	TestFalse(TEXT("owner: no bounce on a shield"), Data.bBounceOnBlock);
	TestTrue(TEXT("on by default"), Data.bEnabled);

	FGSTN_HitBounceOverrides Live;
	Live.MaxConsecutiveBounces = 3.4f;
	Live.ApexHeightCm = 250.f;
	Live.RiseFrames = 0.f;
	Live.BackDistanceCm = 0.f;
	Live.ActionableFrames = 300.f;
	const FGSTN_HitBounceTuning Tuned = FGSTN_HitBounceRules::Resolve(Track, Live);
	TestEqual(TEXT("the console max wins, rounded"), Tuned.MaxConsecutiveBounces, 3);
	TestEqual(TEXT("the console apex wins"), Tuned.ApexHeightCm, 250.f);
	TestEqual(TEXT("rise frames clamp to 1"), Tuned.RiseFrames, 1);
	TestEqual(TEXT("a 0 back distance is a value, not 'use data'"), Tuned.BackDistanceCm, 0.f);
	TestEqual(TEXT("actionable frames clamp to 120"), Tuned.ActionableFrames, 120);

	FGSTN_HitBounceOverrides Off;
	Off.bDisabled = true;
	TestFalse(TEXT("the off switch stops every bounce"), FGSTN_HitBounceRules::CanBounce(0, FGSTN_HitBounceRules::Resolve(Track, Off)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceLimitTest, "GSTNCombat.Core.HitBounce.Limit", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitBounceLimitTest::RunTest(const FString& Parameters)
{
	FGSTN_HitBounceTuning Tuning;
	Tuning.MaxConsecutiveBounces = 2;
	TestTrue(TEXT("first bounce"), FGSTN_HitBounceRules::CanBounce(0, Tuning));
	TestTrue(TEXT("second bounce"), FGSTN_HitBounceRules::CanBounce(1, Tuning));
	TestFalse(TEXT("third: at the limit the move is a normal move"), FGSTN_HitBounceRules::CanBounce(2, Tuning));
	Tuning.MaxConsecutiveBounces = 0;
	TestFalse(TEXT("max 0 never bounces"), FGSTN_HitBounceRules::CanBounce(0, Tuning));

	FGSTN_HitBounceTuning Hits;
	TestTrue(TEXT("a clean hit bounces (owner 2B: any hit)"), FGSTN_HitBounceRules::IsBouncingHit(false, false, false, false, Hits));
	TestFalse(TEXT("a shielded hit does not"), FGSTN_HitBounceRules::IsBouncingHit(false, true, false, false, Hits));
	TestFalse(TEXT("a parried hit does not"), FGSTN_HitBounceRules::IsBouncingHit(false, false, true, false, Hits));
	TestFalse(TEXT("a clank does not"), FGSTN_HitBounceRules::IsBouncingHit(false, false, false, true, Hits));
	TestFalse(TEXT("a ricochet does not"), FGSTN_HitBounceRules::IsBouncingHit(true, false, false, false, Hits));
	Hits.bBounceOnBlock = true;
	TestTrue(TEXT("a shielded hit does when the data says so"), FGSTN_HitBounceRules::IsBouncingHit(false, true, false, false, Hits));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceLaunchTest, "GSTNCombat.Core.HitBounce.Launch", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitBounceLaunchTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitBounceTests;
	FGSTN_HitBounceTuning Tuning;
	const double ModeGravity = 2940.0;
	const FGSTN_HitBounceLaunch Launch = FGSTN_HitBounceRules::ComputeLaunch(Tuning, FVector::UpVector, FVector(1, 0, 0), ModeGravity, Step);
	TestEqual(TEXT("the move lives for the rise"), Launch.Frames, 18);
	TestTrue(TEXT("goes back, against the facing"), Launch.Velocity.X < -1.0);
	TestTrue(TEXT("nothing sideways"), FMath::IsNearlyZero(Launch.Velocity.Y, 1e-3));

	// The apex is the top of the arc and sits at the frame and height asked, within one step's rounding
	const TArray<double> H = Heights(Launch, ModeGravity, 30);
	int32 ApexFrame = 0;
	for (int32 Frame = 1; Frame < H.Num(); ++Frame)
	{
		ApexFrame = H[Frame] > H[ApexFrame] ? Frame : ApexFrame;
	}
	TestTrue(FString::Printf(TEXT("apex at frame ~18 (index %d)"), ApexFrame), FMath::Abs(ApexFrame + 1 - 18) <= 1);
	TestTrue(FString::Printf(TEXT("apex ~150 cm (%.1f)"), H[ApexFrame]), FMath::IsNearlyEqual(H[ApexFrame], 150.0, 3.0));
	const double Back = -Launch.Velocity.X * 18 * Step;
	TestTrue(FString::Printf(TEXT("40 cm back over the rise (%.1f)"), Back), FMath::IsNearlyEqual(Back, 40.0, 0.01));

	// Any gravity: the same bounce under sideways gravity goes along the local up
	const FGSTN_HitBounceLaunch Side = FGSTN_HitBounceRules::ComputeLaunch(Tuning, FVector(0, 1, 0), FVector(1, 0, 0), ModeGravity, Step);
	TestTrue(TEXT("up is the local up"), Side.Velocity.Y > 1000.0 && FMath::IsNearlyZero(Side.Velocity.Z, 1e-3));

	// A changed apex or rise frames moves the arc, the gravity scale follows
	Tuning.ApexHeightCm = 250.f;
	Tuning.RiseFrames = 24;
	const FGSTN_HitBounceLaunch Tall = FGSTN_HitBounceRules::ComputeLaunch(Tuning, FVector::UpVector, FVector(1, 0, 0), ModeGravity, Step);
	const TArray<double> T = Heights(Tall, ModeGravity, 40);
	TestTrue(FString::Printf(TEXT("tuned apex ~250 cm (%.1f)"), T[23]), FMath::IsNearlyEqual(T[23], 250.0, 4.0));

	const FGSTN_HitBounceLaunch NoGravity = FGSTN_HitBounceRules::ComputeLaunch(Tuning, FVector::UpVector, FVector(1, 0, 0), 0.0, Step);
	TestEqual(TEXT("no mode gravity: the scale stays 1"), NoGravity.GravityScale, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitBounceFindTrackTest, "GSTNCombat.Core.HitBounce.FindTrack", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitBounceFindTrackTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 43;
	int32 Index = 7;
	TestNull(TEXT("no track"), FGSTN_HitBounceRules::FindTrack(Data, &Index));
	TestEqual(TEXT("no index"), Index, static_cast<int32>(INDEX_NONE));
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Tags>());
	FGSTN_TimelineTrack_HitBounce Bounce;
	Bounce.StartFrame = 15;
	Bounce.EndFrame = 43;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HitBounce>(Bounce));
	const FGSTN_TimelineTrack_HitBounce* Found = FGSTN_HitBounceRules::FindTrack(Data, &Index);
	TestNotNull(TEXT("found"), Found);
	TestEqual(TEXT("its index"), Index, 1);
	TestEqual(TEXT("an unknown kind for the player"), static_cast<int32>(Found->GetKind()), static_cast<int32>(EGSTN_TimelineTrackKind::None));
	return true;
}

#endif
