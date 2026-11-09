// Automation tests of the minion Core presentation pieces (M8 G7): the interpolator, the hit flash tracker and the visible cut. No world.

#include "GSTN_MinionTestUtils.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Present/GSTN_MinionInterpolator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_MinionPresentSample MakeSample(int32 Frame, const FVector3f& Position, float Yaw = 0.f, float Speed = 0.f, uint8 Flags = 0)
	{
		FGSTN_MinionPresentSample Sample;
		Sample.Frame = Frame;
		Sample.Position = Position;
		Sample.YawDegrees = Yaw;
		Sample.Speed = Speed;
		Sample.Flags = Flags;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentBetweenSnapshotsTest, "GSTNMinion.Core.Present.InterpolatesBetweenSnapshots", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentBetweenSnapshotsTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionSmoothingParams Params;
	FGSTN_MinionInterpolator Interpolator;
	Interpolator.AddSample(MakeSample(100, FVector3f(0.f, 0.f, 0.f), 350.f), 0.0);
	Interpolator.AddSample(MakeSample(104, FVector3f(400.f, 0.f, 40.f), 10.f), 0.05);
	TestFalse(TEXT("an older sample is ignored"), Interpolator.AddSample(MakeSample(103, FVector3f(9999.f, 0.f, 0.f)), 0.06));
	TestEqual(TEXT("two samples kept"), Interpolator.Num(), 2);

	FGSTN_MinionDisplayPose Pose = Interpolator.Interpolate(102.0, 0.1, Params);
	TestTrue(TEXT("interpolated mode"), Pose.Mode == EGSTN_MinionDisplayMode::Interpolated);
	TestTrue(FString::Printf(TEXT("halfway position (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(200.f, 0.f, 20.f), 0.01f));
	TestTrue(FString::Printf(TEXT("shortest-arc yaw 350 -> 10 at half is 0 (got %.3f)"), Pose.YawDegrees), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(0.f, Pose.YawDegrees), 0.01f));
	TestEqual(TEXT("the pose stands for the render frame"), Pose.Frame, 102.0);

	Pose = Interpolator.Interpolate(103.0, 0.11, Params);
	TestTrue(TEXT("three quarters"), Pose.Position.Equals(FVector3f(300.f, 0.f, 30.f), 0.01f));
	TestFalse(TEXT("no blend inside one family"), Pose.bBlending);

	Pose = Interpolator.Interpolate(90.0, 0.12, Params);
	TestTrue(TEXT("before the ring: the oldest sample, held"), Pose.Mode == EGSTN_MinionDisplayMode::Held && Pose.Position.Equals(FVector3f::ZeroVector, 0.01f));

	// A ring of 8: the ninth sample drops the oldest
	for (int32 Index = 0; Index < 8; ++Index)
	{
		Interpolator.AddSample(MakeSample(108 + Index * 4, FVector3f(500.f + Index, 0.f, 0.f)), 0.2 + Index * 0.01);
	}
	TestEqual(TEXT("ring holds 8"), Interpolator.Num(), FGSTN_MinionInterpolator::RingSize);
	TestEqual(TEXT("oldest is the first of the last eight"), Interpolator.At(0).Frame, 108);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentExtrapolationCapTest, "GSTNMinion.Core.Present.ExtrapolationCap", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentExtrapolationCapTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionSmoothingParams Params; // 16 ms frames, 150 ms cap
	FGSTN_MinionInterpolator Interpolator;
	Interpolator.AddSample(MakeSample(100, FVector3f(0.f, 0.f, 0.f), 90.f, 300.f), 0.0);
	Interpolator.AddSample(MakeSample(104, FVector3f(0.f, 19.2f, 0.f), 90.f, 300.f), 0.064);

	// 5 frames = 80 ms past the newest sample, along +Y (yaw 90) at 300 cm/s: 24 cm
	FGSTN_MinionDisplayPose Pose = Interpolator.Interpolate(109.0, 0.1, Params);
	TestTrue(TEXT("extrapolated"), Pose.Mode == EGSTN_MinionDisplayMode::Extrapolated);
	TestTrue(FString::Printf(TEXT("80 ms at 300 cm/s (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(0.f, 19.2f + 24.f, 0.f), 0.05f));

	// 30 frames = 480 ms past it: capped at 150 ms (45 cm), then held there
	Pose = Interpolator.Interpolate(134.0, 0.2, Params);
	TestTrue(TEXT("held past the cap"), Pose.Mode == EGSTN_MinionDisplayMode::Held);
	TestTrue(FString::Printf(TEXT("stops at the cap: 150 ms of motion (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(0.f, 19.2f + 45.f, 0.f), 0.05f));
	TestTrue(TEXT("its frame is the newest plus the cap"), FMath::IsNearlyEqual(Pose.Frame, 104.0 + 150.0 / 16.0, 1e-6));
	TestFalse(TEXT("extrapolated and held are one family: no blend"), Pose.bBlending);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentBlendTest, "GSTNMinion.Core.Present.DeadReckoningBlendConverges", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentBlendTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionSmoothingParams Params; // 200 ms blend
	FGSTN_MinionInterpolator Interpolator;
	Interpolator.AddSample(MakeSample(100, FVector3f(0.f, 0.f, 0.f)), 0.0);
	FGSTN_MinionDisplayPose Pose = Interpolator.DeadReckon(0.0, Params);
	TestTrue(TEXT("first pose snaps"), Pose.Position.Equals(FVector3f::ZeroVector) && !Pose.bBlending);

	// A T2-like update one second later moves the target 100 cm: the jump is blended out over 200 ms, never popped
	Interpolator.AddSample(MakeSample(162, FVector3f(100.f, 0.f, 0.f)), 1.0);
	TestTrue(TEXT("62-frame gap reads as T2"), Interpolator.GetInferredTier(Params) == EGSTN_MinionLODTier::Far);
	Pose = Interpolator.DeadReckon(1.0, Params);
	TestTrue(FString::Printf(TEXT("blend starts at the shown pose (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f::ZeroVector, 0.01f) && Pose.bBlending);
	Pose = Interpolator.DeadReckon(1.1, Params);
	TestTrue(FString::Printf(TEXT("halfway after 100 ms (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(50.f, 0.f, 0.f), 0.01f));
	Pose = Interpolator.DeadReckon(1.2, Params);
	TestTrue(FString::Printf(TEXT("converged after 200 ms (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(100.f, 0.f, 0.f), 0.01f) && !Pose.bBlending);
	Pose = Interpolator.DeadReckon(1.5, Params);
	TestTrue(TEXT("stays converged"), Pose.Position.Equals(FVector3f(100.f, 0.f, 0.f), 0.01f));

	// A moving sample: dead reckoning runs ahead on local time from its arrival
	Interpolator.AddSample(MakeSample(174, FVector3f(100.f, 0.f, 0.f), 0.f, 200.f), 2.0);
	TestTrue(TEXT("12-frame gap reads as T1"), Interpolator.GetInferredTier(Params) == EGSTN_MinionLODTier::Near);
	Interpolator.DeadReckon(2.0, Params);
	Pose = Interpolator.DeadReckon(2.1, Params);
	TestTrue(FString::Printf(TEXT("100 ms at 200 cm/s ahead of the sample (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(120.f, 0.f, 0.f), 0.01f));
	TestTrue(TEXT("dead reckoned"), Pose.Mode == EGSTN_MinionDisplayMode::DeadReckoned);
	// G9b: never further than the sender's own interval (12 frames = 192 ms) when the next sample is late
	Pose = Interpolator.DeadReckon(2.5, Params);
	TestTrue(FString::Printf(TEXT("capped at 192 ms ahead (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(138.4f, 0.f, 0.f), 0.01f));

	// G9b: a sender slower than T1 (62 frames) holds its newest sample (FarDeadReckoningCapMs 0), even with a speed on it
	Interpolator.AddSample(MakeSample(236, FVector3f(300.f, 0.f, 0.f), 0.f, 200.f), 3.0);
	Interpolator.DeadReckon(3.0, Params);
	Pose = Interpolator.DeadReckon(3.6, Params);
	TestTrue(FString::Printf(TEXT("a far sender is not run ahead (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(300.f, 0.f, 0.f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentTeleportTest, "GSTNMinion.Core.Present.TeleportSnaps", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentTeleportTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionSmoothingParams Params;
	FGSTN_MinionInterpolator Interpolator;
	Interpolator.AddSample(MakeSample(100, FVector3f(0.f, 0.f, 0.f)), 0.0);
	Interpolator.DeadReckon(0.0, Params);
	Interpolator.AddSample(MakeSample(130, FVector3f(100.f, 0.f, 0.f)), 0.5);
	Interpolator.DeadReckon(0.5, Params);

	// Mid-blend, a teleport: the ring restarts and the pose is the sample, no blend
	TestTrue(TEXT("a teleport is always taken, even with an older frame"), Interpolator.AddSample(MakeSample(90, FVector3f(5000.f, 0.f, 0.f), 0.f, 0.f, GSTNMinion::NetFlags::Teleport), 0.55));
	TestEqual(TEXT("the ring restarted"), Interpolator.Num(), 1);
	FGSTN_MinionDisplayPose Pose = Interpolator.DeadReckon(0.55, Params);
	TestTrue(FString::Printf(TEXT("snapped (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(5000.f, 0.f, 0.f), 0.01f) && !Pose.bBlending);

	// The same on the interpolated path: a teleport between T0 samples does not slide across the map
	FGSTN_MinionInterpolator T0;
	T0.AddSample(MakeSample(200, FVector3f(0.f, 0.f, 0.f)), 0.0);
	T0.AddSample(MakeSample(204, FVector3f(10.f, 0.f, 0.f)), 0.06);
	T0.Interpolate(202.0, 0.07, Params);
	T0.AddSample(MakeSample(208, FVector3f(9000.f, 0.f, 0.f), 0.f, 0.f, GSTNMinion::NetFlags::Teleport), 0.1);
	Pose = T0.Interpolate(206.0, 0.11, Params);
	TestTrue(FString::Printf(TEXT("held at the teleport target, not halfway (got %s)"), *Pose.Position.ToString()), Pose.Position.Equals(FVector3f(9000.f, 0.f, 0.f), 0.01f));
	TestFalse(TEXT("no blend into a teleport"), Pose.bBlending);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentTierTest, "GSTNMinion.Core.Present.InfersTierFromSampleGap", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentTierTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionSmoothingParams Params;
	FGSTN_MinionInterpolator Interpolator;
	Interpolator.AddSample(MakeSample(100, FVector3f::ZeroVector), 0.0);
	TestTrue(TEXT("one sample: Far (dead reckoning)"), Interpolator.GetInferredTier(Params) == EGSTN_MinionLODTier::Far);
	Interpolator.AddSample(MakeSample(104, FVector3f::ZeroVector), 0.0);
	TestTrue(TEXT("15 Hz gap: Combat"), Interpolator.GetInferredTier(Params) == EGSTN_MinionLODTier::Combat);
	// Params.FixedStepMs 16: 0.05 s of silence is ~3 frames (still T0), 0.3 s ~18 frames (T1), 1 s ~62 frames (T2)
	TestTrue(TEXT("silent 0.05 s: still Combat"), Interpolator.GetInferredTier(Params, 0.05) == EGSTN_MinionLODTier::Combat);
	TestTrue(TEXT("silent 0.3 s: Near"), Interpolator.GetInferredTier(Params, 0.3) == EGSTN_MinionLODTier::Near);
	TestTrue(TEXT("silent 1 s: Far"), Interpolator.GetInferredTier(Params, 1.0) == EGSTN_MinionLODTier::Far);
	Interpolator.AddSample(MakeSample(117, FVector3f::ZeroVector), 0.0);
	TestTrue(TEXT("5 Hz gap: Near"), Interpolator.GetInferredTier(Params) == EGSTN_MinionLODTier::Near);
	Interpolator.AddSample(MakeSample(180, FVector3f::ZeroVector), 0.0);
	TestTrue(TEXT("1 Hz gap: Far"), Interpolator.GetInferredTier(Params) == EGSTN_MinionLODTier::Far);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentConfirmWindowTest, "GSTNMinion.Core.Present.ConfirmWindowFollowsPing", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentConfirmWindowTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionFlashParams Params;
	TestEqual(TEXT("no ping (host, no player state): the base 500 ms window"), Params.ResolveConfirmWindow(0.f), 0.5f);
	TestEqual(TEXT("a low ping still gets the 750 ms floor"), Params.ResolveConfirmWindow(0.05f), 0.75f);
	TestEqual(TEXT("a high ping widens it: 4 x 0.2 + 0.25"), Params.ResolveConfirmWindow(0.2f), 1.05f);
	TestEqual(TEXT("and it is capped"), Params.ResolveConfirmWindow(2.f), Params.MaxConfirmWindowSeconds);

	// A confirm inside 500 ms is the same under either window; a late one (600 ms) is only caught by the wide window
	FGSTN_MinionFlashParams Wide = Params;
	Wide.ConfirmWindowSeconds = Params.ResolveConfirmWindow(0.06f);
	FGSTN_MinionHitFlash Narrow;
	FGSTN_MinionHitFlash Wider;
	for (FGSTN_MinionHitFlash* Flash : { &Narrow, &Wider })
	{
		Flash->ObserveCounter(0, 0.0, Params);
	}
	Narrow.OnPredictedHit(1.0, Params);
	Wider.OnPredictedHit(1.0, Wide);
	Narrow.ObserveCounter(1, 1.6, Params);
	Wider.ObserveCounter(1, 1.6, Wide);
	TestTrue(TEXT("500 ms window: a 600 ms confirm expired, then counted remote"), Narrow.NumExpired == 1 && Narrow.NumRemote == 1 && Narrow.NumConfirmed == 0);
	TestTrue(TEXT("adaptive window: the same confirm lands in the window"), Wider.NumConfirmed == 1 && Wider.NumExpired == 0 && Wider.NumRemote == 0);

	FGSTN_MinionHitFlash Early;
	Early.ObserveCounter(0, 0.0, Params);
	Early.OnPredictedHit(1.0, Wide);
	Early.ObserveCounter(1, 1.3, Wide);
	TestTrue(TEXT("a confirm inside 500 ms never depended on the window"), Early.NumConfirmed == 1 && Early.NumExpired == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentFlashTest,"GSTNMinion.Core.Present.FlashConfirmAndCancelByHitCounter", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentFlashTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionFlashParams Params; // 0.12 s flash, 0.1 s pause, 0.5 s window
	FGSTN_MinionHitFlash Flash;
	Flash.ObserveCounter(6, 0.0, Params);
	TestTrue(TEXT("the first counter is a baseline: no flash"), Flash.GetState() == EGSTN_MinionFlashState::None && Flash.GetFlash(0.0, Params) == 0.f);

	// Predicted, then confirmed by the counter inside the window (7 -> 0 wraps: still one hit)
	Flash.OnPredictedHit(1.0, Params);
	TestTrue(TEXT("predicted: full flash at once"), Flash.GetState() == EGSTN_MinionFlashState::Predicted && Flash.GetFlash(1.0, Params) == 1.f);
	TestTrue(TEXT("predicted: mesh paused"), Flash.IsPaused(1.05));
	Flash.ObserveCounter(7, 1.2, Params);
	TestTrue(TEXT("confirmed by HitCounter"), Flash.GetState() == EGSTN_MinionFlashState::Confirmed && Flash.NumConfirmed == 1);
	TestEqual(TEXT("a confirm is not a second hit"), Flash.NumRemote, 0);
	TestEqual(TEXT("nothing pending"), Flash.GetPendingPredictions(), 0);

	// Predicted, then the attacker's entry vanished (a correction): everything ends at once
	Flash.OnPredictedHit(2.0, Params);
	Flash.OnPredictionCancelled(2.03);
	TestTrue(TEXT("cancelled"), Flash.GetState() == EGSTN_MinionFlashState::Cancelled && Flash.NumCancelled == 1);
	TestEqual(TEXT("cancelled: flash off"), Flash.GetFlash(2.04, Params), 0.f);
	TestFalse(TEXT("cancelled: mesh resumes"), Flash.IsPaused(2.04));

	// Predicted, never confirmed: ends after the 500 ms window
	Flash.OnPredictedHit(3.0, Params);
	Flash.Tick(3.4);
	TestTrue(TEXT("still waiting inside the window"), Flash.GetState() == EGSTN_MinionFlashState::Predicted);
	Flash.Tick(3.6);
	TestTrue(TEXT("expired after the window"), Flash.GetState() == EGSTN_MinionFlashState::Expired && Flash.NumExpired == 1);

	// A counter change with nothing pending: another player's hit, same effects
	Flash.ObserveCounter(0, 4.0, Params);
	TestTrue(TEXT("remote hit flashes"), Flash.GetState() == EGSTN_MinionFlashState::Remote && Flash.NumRemote == 1 && Flash.GetFlash(4.0, Params) == 1.f);
	TestTrue(TEXT("flash fades out"), Flash.GetFlash(4.2, Params) == 0.f);

	// Two predictions, one counter step: one confirmed, one still pending
	Flash.OnPredictedHit(5.0, Params);
	Flash.OnPredictedHit(5.01, Params);
	Flash.ObserveCounter(1, 5.1, Params);
	TestEqual(TEXT("one of two confirmed"), Flash.GetPendingPredictions(), 1);
	Flash.ObserveCounter(2, 5.2, Params);
	TestEqual(TEXT("both confirmed"), Flash.NumConfirmed, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentNearestTest, "GSTNMinion.Core.Present.MaxVisibleNearestFirst", GSTNMinionTest::Flags)
bool FGSTN_MinionPresentNearestTest::RunTest(const FString& Parameters)
{
	TArray<int32> Visible;
	const TArray<float> Distances = { 900.f, 100.f, 400.f, 2500.f, 100.f };
	GSTNMinionPresent::SelectNearest(Distances, TArray<bool>(), 3, 0.f, Visible);
	TestEqual(TEXT("three kept"), Visible.Num(), 3);
	TestTrue(TEXT("nearest first, ties by index"), Visible.Num() == 3 && Visible[0] == 1 && Visible[1] == 4 && Visible[2] == 2);

	GSTNMinionPresent::SelectNearest(Distances, TArray<bool>(), 60, 0.f, Visible);
	TestEqual(TEXT("cap above the count keeps all"), Visible.Num(), 5);
	GSTNMinionPresent::SelectNearest(Distances, TArray<bool>(), 0, 0.f, Visible);
	TestEqual(TEXT("cap 0 keeps none"), Visible.Num(), 0);

	// The sticky margin: a shown puppet slightly farther keeps its place, a much farther one loses it
	const TArray<float> Close = { 100.f, 105.f };
	const TArray<bool> Shown = { false, true };
	GSTNMinionPresent::SelectNearest(Close, Shown, 1, 0.1f, Visible);
	TestTrue(TEXT("shown puppet within the margin stays"), Visible.Num() == 1 && Visible[0] == 1);
	const TArray<float> Far = { 100.f, 200.f };
	GSTNMinionPresent::SelectNearest(Far, Shown, 1, 0.1f, Visible);
	TestTrue(TEXT("shown puppet beyond the margin goes"), Visible.Num() == 1 && Visible[0] == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
