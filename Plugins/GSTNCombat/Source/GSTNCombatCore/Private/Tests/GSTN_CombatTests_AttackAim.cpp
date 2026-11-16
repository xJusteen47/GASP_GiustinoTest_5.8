#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// Stick priority on attacks (owner 2026-11-16: "stick right while facing left, the move goes right", with the reverse tricks). Goldens of the pure
// start aim / reverse decision the combat tick calls before the steering (FGSTN_StartupSteer::ResolveOriginAim). Camera at yaw 0: stick right = yaw 90,
// stick up = yaw 0, stick down = yaw 180.

namespace GSTNAttackAimTest
{
	/** 40 frames, one hitbox [ActiveStart, ActiveStart + 4) */
	FGSTN_CombatTimelineData MakeAttack(int32 ActiveStart = 12)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 40;
		Data.FirstActionableFrame = 34;
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = ActiveStart;
		Track.EndFrame = ActiveStart + 4;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		return Data;
	}

	FGSTN_AimFrameInput Frame(int32 TimelineFrame, float Origin, const FVector2D& MoveStick, float Camera = 0.f)
	{
		FGSTN_AimFrameInput In;
		In.TimelineFrame = TimelineFrame;
		In.OriginYawDeg = Origin;
		In.MoveStick = MoveStick;
		In.CameraYawDeg = Camera;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackAimPolicyTest, "GSTNCombat.Core.AttackAim.Policy", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AttackAimPolicyTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAttackAimTest;
	using S = FGSTN_StartupSteer;
	TestTrue(TEXT("every move defaults to Auto"), FGSTN_CombatTimelineData().AimPolicy == EGSTN_AimPolicy::Auto);
	TestTrue(TEXT("Auto attack: aim or move stick"), S::ResolveAimPolicy(MakeAttack()) == EGSTN_AimPolicy::SnapIfAimOrTilt);
	TestTrue(TEXT("Auto without a hitbox (roll, dodge): keeps facing"), S::ResolveAimPolicy(FGSTN_CombatTimelineData()) == EGSTN_AimPolicy::KeepFacing);
	FGSTN_CombatTimelineData GetUp = MakeAttack();
	GetUp.bIgnoreMoveInput = true;
	TestTrue(TEXT("Auto get-up attack (stick ignored): keeps facing"), S::ResolveAimPolicy(GetUp) == EGSTN_AimPolicy::KeepFacing);
	FGSTN_CombatTimelineData Explicit = MakeAttack();
	Explicit.AimPolicy = EGSTN_AimPolicy::KeepFacing;
	TestTrue(TEXT("an explicit policy is kept"), S::ResolveAimPolicy(Explicit) == EGSTN_AimPolicy::KeepFacing);
	TestEqual(TEXT("stick right = yaw 90"), S::StickYaw(FVector2D(1.0, 0.0), 0.f), 90.f, 1.e-3f);
	TestEqual(TEXT("camera turns the stick"), S::StickYaw(FVector2D(0.0, 1.0), 45.f), 45.f, 1.e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackAimSnapTest, "GSTNCombat.Core.AttackAim.Snap", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AttackAimSnapTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAttackAimTest;
	using S = FGSTN_StartupSteer;
	constexpr float Tol = 1.e-2f;
	const FVector2D Right(1.0, 0.0);
	float Yaw = 0.f;
	FGSTN_CombatTimelineData Attack = MakeAttack();

	// The owner's case: facing left (-90), stick right, attack: the move goes right
	TestTrue(TEXT("stick right while facing left: snaps"), S::ResolveOriginAim(Attack, Frame(0, -90.f, Right), Yaw) == EGSTN_AimTurn::Snap);
	TestEqual(TEXT("to the right"), Yaw, 90.f, Tol);
	TestTrue(TEXT("stick at rest: keeps the facing"), S::ResolveOriginAim(Attack, Frame(0, -90.f, FVector2D(0.1, 0.0)), Yaw) == EGSTN_AimTurn::None);
	TestTrue(TEXT("already facing the stick: no turn"), S::ResolveOriginAim(Attack, Frame(0, 90.f, Right), Yaw) == EGSTN_AimTurn::None);
	TestTrue(TEXT("frame 1 still snaps (a move the ability graph started)"), S::ResolveOriginAim(Attack, Frame(1, 0.f, Right), Yaw) == EGSTN_AimTurn::Snap);
	TestTrue(TEXT("frame 2: the start aim is over"), S::ResolveOriginAim(Attack, Frame(2, 0.f, Right), Yaw) == EGSTN_AimTurn::None);

	// The reticle's aim stick wins over the move stick, only with the reticle on
	FGSTN_AimFrameInput Both = Frame(0, 0.f, Right);
	Both.AimStick = FVector2D(-1.0, 0.0);
	TestTrue(TEXT("reticle off: move stick"), S::ResolveOriginAim(Attack, Both, Yaw) == EGSTN_AimTurn::Snap && FMath::IsNearlyEqual(Yaw, 90.f, Tol));
	Both.bPrecisionAim = true;
	TestTrue(TEXT("reticle on: aim stick"), S::ResolveOriginAim(Attack, Both, Yaw) == EGSTN_AimTurn::Snap && FMath::IsNearlyEqual(Yaw, -90.f, Tol));

	// Cap, policies
	Attack.MaxTurnDegrees = 45.f;
	TestTrue(TEXT("capped"), S::ResolveOriginAim(Attack, Frame(0, 0.f, Right), Yaw) == EGSTN_AimTurn::Snap);
	TestEqual(TEXT("at most MaxTurnDegrees"), Yaw, 45.f, Tol);
	Attack.MaxTurnDegrees = 180.f;
	Attack.AimPolicy = EGSTN_AimPolicy::KeepFacing;
	Attack.Reverse.Frames = 0;
	TestTrue(TEXT("KeepFacing (a back air): no turn"), S::ResolveOriginAim(Attack, Frame(0, -90.f, Right), Yaw) == EGSTN_AimTurn::None);
	Attack.AimPolicy = EGSTN_AimPolicy::SnapToStick;
	FGSTN_AimFrameInput AimOnly = Frame(0, 0.f, FVector2D::ZeroVector);
	AimOnly.AimStick = Right;
	AimOnly.bPrecisionAim = true;
	TestTrue(TEXT("SnapToStick ignores the aim stick"), S::ResolveOriginAim(Attack, AimOnly, Yaw) == EGSTN_AimTurn::None);
	Attack.AimPolicy = EGSTN_AimPolicy::Camera;
	Attack.TurnUntilFrame = 6;
	TestTrue(TEXT("Camera: follows the camera"), S::ResolveOriginAim(Attack, Frame(5, 0.f, FVector2D::ZeroVector, 30.f), Yaw) == EGSTN_AimTurn::Snap && FMath::IsNearlyEqual(Yaw, 30.f, Tol));
	TestTrue(TEXT("Camera: until TurnUntilFrame"), S::ResolveOriginAim(Attack, Frame(7, 0.f, FVector2D::ZeroVector, 30.f), Yaw) == EGSTN_AimTurn::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackAimReverseTest, "GSTNCombat.Core.AttackAim.Reverse", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AttackAimReverseTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAttackAimTest;
	using S = FGSTN_StartupSteer;
	constexpr float Tol = 1.e-2f;
	const FVector2D Back(0.0, -1.0);
	float Yaw = 0.f;
	FGSTN_CombatTimelineData Attack = MakeAttack();

	TestEqual(TEXT("default window: frames 1..3"), S::GetReverseEndFrame(Attack), 4);
	// Facing forward (0) and the move started; a hard flick back in frames 1..3 turns it
	TestTrue(TEXT("flick back on frame 2: reverse"), S::ResolveOriginAim(Attack, Frame(2, 0.f, Back), Yaw) == EGSTN_AimTurn::Reverse);
	TestEqual(TEXT("to the stick"), FMath::Abs(Yaw), 180.f, Tol);
	TestTrue(TEXT("frame 4: too late"), S::ResolveOriginAim(Attack, Frame(4, 0.f, Back), Yaw) == EGSTN_AimTurn::None);
	TestTrue(TEXT("a soft push back is not a flick"), S::ResolveOriginAim(Attack, Frame(2, 0.f, Back * 0.5), Yaw) == EGSTN_AimTurn::None);
	TestTrue(TEXT("sideways (90) is not back"), S::ResolveOriginAim(Attack, Frame(2, 0.f, FVector2D(1.0, 0.0)), Yaw) == EGSTN_AimTurn::None);
	// Frame 0 belongs to the start aim: held back as the move comes out is a turnaround (Snap, momentum kept)
	TestTrue(TEXT("frame 0: snap, not reverse"), S::ResolveOriginAim(Attack, Frame(0, 0.f, Back), Yaw) == EGSTN_AimTurn::Snap);
	// Works on a KeepFacing move too (a special with only the B-reverse), unless its window is 0
	Attack.AimPolicy = EGSTN_AimPolicy::KeepFacing;
	TestTrue(TEXT("KeepFacing move still reverses"), S::ResolveOriginAim(Attack, Frame(1, 0.f, Back), Yaw) == EGSTN_AimTurn::Reverse);
	Attack.Reverse.Frames = 0;
	TestTrue(TEXT("window 0: never"), S::ResolveOriginAim(Attack, Frame(1, 0.f, Back), Yaw) == EGSTN_AimTurn::None);

	// Never into the active frames; never for a roll or a get-up
	FGSTN_CombatTimelineData Fast = MakeAttack(2);
	TestEqual(TEXT("cut at the first active frame"), S::GetReverseEndFrame(Fast), 2);
	TestTrue(TEXT("active frame 2: no reverse"), S::ResolveOriginAim(Fast, Frame(2, 0.f, Back), Yaw) == EGSTN_AimTurn::None);
	TestEqual(TEXT("no hitbox: no window"), S::GetReverseEndFrame(FGSTN_CombatTimelineData()), 0);
	FGSTN_CombatTimelineData GetUp = MakeAttack();
	GetUp.bIgnoreMoveInput = true;
	TestEqual(TEXT("stick ignored: no window"), S::GetReverseEndFrame(GetUp), 0);
	return true;
}

#endif
