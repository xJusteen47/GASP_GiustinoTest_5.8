#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// Per-move steering (owner 2026-11-14, FGSTN_SteerWindow): which stick, in which frames, the hold and the lock. Goldens of the pure per-frame
// decision the sim, the Mover input producer and the arrows all call.

namespace GSTNSteerWindowTest
{
	/** 40 frames, one hitbox [12, 16): first active 12, hold ends at 16 */
	FGSTN_CombatTimelineData MakeMove(EGSTN_SteerSource Source, float Cap, float Rate = 0.f)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 40;
		Data.FirstActionableFrame = 34;
		Data.MaxSteerDegrees = Cap;
		Data.SteerRateDegPerSecond = Rate;
		Data.Steer.Source = Source;
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 12;
		Track.EndFrame = 16;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		return Data;
	}

	FGSTN_SteerFrameInput Frame(int32 TimelineFrame, float Current, const FVector2D& Stick, float Step = 0.f)
	{
		FGSTN_SteerFrameInput In;
		In.TimelineFrame = TimelineFrame;
		In.StartYawDeg = 0.f;
		In.CurrentYawDeg = Current;
		In.Stick = Stick;
		In.bSourceActive = true;
		In.StepSeconds = Step;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SteerWindowSourceTest, "GSTNCombat.Core.StartupSteer.Window.Source", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SteerWindowSourceTest::RunTest(const FString& Parameters)
{
	using S = FGSTN_StartupSteer;
	const FVector2D Move(1.0, 0.0);
	const FVector2D Aim(0.0, 1.0);
	FVector2D Out;

	TestTrue(TEXT("old moves keep the reticle source"), FGSTN_CombatTimelineData().Steer.Source == EGSTN_SteerSource::AimStickWithReticle);
	TestFalse(TEXT("reticle source, reticle off: nothing"), S::PickStick(EGSTN_SteerSource::AimStickWithReticle, Move, Aim, false, Out));
	TestTrue(TEXT("reticle source, reticle on"), S::PickStick(EGSTN_SteerSource::AimStickWithReticle, Move, Aim, true, Out) && Out == Aim);
	TestTrue(TEXT("move stick, reticle on or off"), S::PickStick(EGSTN_SteerSource::MoveStick, Move, Aim, true, Out) && Out == Move);
	TestTrue(TEXT("either: aim stick wins with the reticle"), S::PickStick(EGSTN_SteerSource::MoveOrAimStick, Move, Aim, true, Out) && Out == Aim);
	TestTrue(TEXT("either: move stick without the reticle"), S::PickStick(EGSTN_SteerSource::MoveOrAimStick, Move, Aim, false, Out) && Out == Move);
	TestTrue(TEXT("either: aim stick at rest falls back"), S::PickStick(EGSTN_SteerSource::MoveOrAimStick, Move, FVector2D(0.1, 0.0), true, Out) && Out == Move);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SteerWindowPhasesTest, "GSTNCombat.Core.StartupSteer.Window.Phases", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SteerWindowPhasesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSteerWindowTest;
	using S = FGSTN_StartupSteer;
	constexpr float Tol = 1.e-2f;
	const FVector2D Right(1.0, 0.0);
	float Yaw = 0.f;
	bool bPushed = false;

	FGSTN_CombatTimelineData Move = MakeMove(EGSTN_SteerSource::MoveStick, 45.f);
	int32 Start = 0;
	int32 End = 0;
	S::GetSteerWindow(Move, Start, End);
	TestEqual(TEXT("window ends at the first active frame"), End, 12);
	TestEqual(TEXT("hold ends with the last active frame"), S::GetHoldEndFrame(Move), 16);

	// Steering: right stick aims at 90, clamped to 45
	TestTrue(TEXT("steers in the window"), S::ResolveFrame(Move, Frame(0, 0.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::Steering && bPushed);
	TestEqual(TEXT("clamped to the cap"), Yaw, 45.f, Tol);
	// Stick released: the move stick keeps where it got to, never snaps back to the origin
	TestTrue(TEXT("released, still steering"), S::ResolveFrame(Move, Frame(5, 30.f, FVector2D::ZeroVector), Yaw, bPushed) == EGSTN_SteerPhase::Steering && !bPushed);
	TestEqual(TEXT("released keeps the aim"), Yaw, 30.f, Tol);
	// Locked from the first active frame to the end of the last one, then free
	TestTrue(TEXT("locked at the first active frame"), S::ResolveFrame(Move, Frame(12, 30.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::Locked && !bPushed);
	TestEqual(TEXT("lock holds the facing"), Yaw, 30.f, Tol);
	TestTrue(TEXT("locked through the active frames"), S::ResolveFrame(Move, Frame(15, 30.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::Locked);
	TestTrue(TEXT("free after the active frames"), S::ResolveFrame(Move, Frame(16, 30.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::None);

	// A late start: held before it
	Move.Steer.StartFrame = 4;
	TestTrue(TEXT("held before the window"), S::ResolveFrame(Move, Frame(2, 10.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::Before);
	TestEqual(TEXT("before holds the facing"), Yaw, 10.f, Tol);
	// Without the hold the move stick sources leave the movement mode alone outside the window
	Move.Steer.bHoldFacing = false;
	TestTrue(TEXT("no hold: nothing before"), S::ResolveFrame(Move, Frame(2, 10.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::None);
	TestTrue(TEXT("no hold: nothing after the lock"), S::ResolveFrame(Move, Frame(13, 10.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::None);

	// An explicit end frame overrides the first active frame
	Move.Steer.EndFrame = 8;
	S::GetSteerWindow(Move, Start, End);
	TestEqual(TEXT("explicit end"), End, 8);

	// The reticle source as before: nothing with the reticle off, back to the origin with the stick released, nothing after the window
	const FGSTN_CombatTimelineData Old = MakeMove(EGSTN_SteerSource::AimStickWithReticle, 20.f);
	FGSTN_SteerFrameInput Off = Frame(0, 0.f, Right);
	Off.bSourceActive = false;
	TestTrue(TEXT("reticle off"), S::ResolveFrame(Old, Off, Yaw, bPushed) == EGSTN_SteerPhase::None);
	TestTrue(TEXT("reticle on"), S::ResolveFrame(Old, Frame(0, 0.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::Steering);
	TestEqual(TEXT("reticle cap"), Yaw, 20.f, Tol);
	S::ResolveFrame(Old, Frame(3, 15.f, FVector2D::ZeroVector), Yaw, bPushed);
	TestEqual(TEXT("reticle released: origin"), Yaw, 0.f, Tol);
	TestTrue(TEXT("reticle: no lock hold"), S::ResolveFrame(Old, Frame(12, 15.f, Right), Yaw, bPushed) == EGSTN_SteerPhase::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SteerWindowRateTest, "GSTNCombat.Core.StartupSteer.Window.Rate", GSTN_CORE_TEST_FLAGS)
bool FGSTN_SteerWindowRateTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSteerWindowTest;
	using S = FGSTN_StartupSteer;
	constexpr float Tol = 1.e-2f;
	constexpr float Step = 0.016f; // 62.5 Hz
	const FVector2D Back(0.0, -1.0);
	float Yaw = 0.f;
	bool bPushed = false;

	// Signature aerial: 180 in the first 8 frames at 900 deg/s = 14.4 deg per frame. Stick straight back from facing 0 turns the short way
	FGSTN_CombatTimelineData Signature = MakeMove(EGSTN_SteerSource::MoveOrAimStick, 180.f, 900.f);
	Signature.Steer.EndFrame = 8;
	S::ResolveFrame(Signature, Frame(0, 0.f, Back, Step), Yaw, bPushed);
	TestEqual(TEXT("one frame of 900 deg/s"), FMath::Abs(Yaw), 14.4f, Tol);
	float Current = 0.f;
	for (int32 F = 0; F < 8; ++F)
	{
		S::ResolveFrame(Signature, Frame(F, Current, Back, Step), Current, bPushed);
	}
	TestEqual(TEXT("eight frames: 115.2 deg"), FMath::Abs(Current), 115.2f, Tol);
	TestTrue(TEXT("locked at frame 8"), S::ResolveFrame(Signature, Frame(8, Current, Back, Step), Yaw, bPushed) == EGSTN_SteerPhase::Locked);
	TestEqual(TEXT("lock keeps the turn"), Yaw, Current, Tol);

	// Wide cone (135): from +120 to a stick at -120 the step goes through the front (inside the cone), not the short way through the back
	const float Stepped = S::RateLimitInCone(0.f, 120.f, -120.f, 135.f, 10.f);
	TestEqual(TEXT("wide cone steps toward the front"), Stepped, 110.f, Tol);
	TestEqual(TEXT("narrow cone keeps the short way"), S::RateLimitInCone(0.f, 10.f, -10.f, 45.f, 5.f), 5.f, Tol);

	// The rate curve: a key of 0 at the end freezes the turn there (heavy toward the lock)
	FGSTN_CombatTimelineData Heavy = MakeMove(EGSTN_SteerSource::MoveStick, 90.f, 600.f);
	FRichCurve* Curve = Heavy.Steer.RateScale.GetRichCurve();
	Curve->AddKey(0.f, 1.f);
	Curve->AddKey(1.f, 0.f);
	S::ResolveFrame(Heavy, Frame(0, 0.f, FVector2D(1.0, 0.0), Step), Yaw, bPushed);
	TestEqual(TEXT("full rate at the start"), Yaw, 9.6f, Tol);
	S::ResolveFrame(Heavy, Frame(6, 0.f, FVector2D(1.0, 0.0), Step), Yaw, bPushed);
	TestEqual(TEXT("half rate halfway"), Yaw, 4.8f, Tol);

	// Same input, same answer: the decision is a pure function (rollback)
	float A = 0.f;
	float B = 0.f;
	S::ResolveFrame(Signature, Frame(3, 40.f, Back, Step), A, bPushed);
	S::ResolveFrame(Signature, Frame(3, 40.f, Back, Step), B, bPushed);
	TestEqual(TEXT("deterministic"), A, B);
	return true;
}

#endif
