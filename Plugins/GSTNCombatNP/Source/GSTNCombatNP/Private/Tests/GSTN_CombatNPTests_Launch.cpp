// Automation tests of the surface launch of a non-tumbling combatant (M4 H11b). No world: the launch is the pure composition of
// FGSTN_SurfaceLaunch over hand built params, the flags are the jump state's reset rules, the wire is the move's NetSerialize.

#include "Misc/AutomationTest.h"
#include "GSTN_SurfaceTypes.h"
#include "Jump/GSTN_JumpLogic.h"
#include "Movement/GSTN_LayeredMove_Launch.h"
#include "Movement/GSTN_SurfaceLaunch.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	FGSTN_SurfaceLaunchParams FixedParams(float Speed)
	{
		FGSTN_SurfaceLaunchParams Params;
		Params.DirectionMode = EGSTN_SurfaceLaunchDirection::LocalDirection;
		Params.LocalDirection = FVector::UpVector;
		Params.SpeedMode = EGSTN_SurfaceLaunchSpeed::Fixed;
		Params.Speed = Speed;
		Params.bKeepTangentialVelocity = true;
		Params.bAddSurfaceVelocity = false;
		return Params;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LaunchFromSurfaceParamsTest, "GSTNCombat.NP.Hits.Launch.FromSurfaceParams", GSTN_NP_TEST_FLAGS)
bool FGSTN_LaunchFromSurfaceParamsTest::RunTest(const FString& Parameters)
{
	const FTransform Flat = FTransform::Identity;
	const FVector Falling(300.0, 0.0, -900.0);

	// Fixed speed along local up, the velocity along the face kept
	FGSTN_SurfaceLaunchParams Params = FixedParams(1500.f);
	TestTrue(TEXT("Fixed launch keeps the tangential velocity"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, Falling, FVector::ZeroVector).Equals(FVector(300.0, 0.0, 1500.0), 1.e-3));

	// A hard replacement drops it
	Params.bKeepTangentialVelocity = false;
	TestTrue(TEXT("Without bKeepTangentialVelocity the launch replaces the whole velocity"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, Falling, FVector::ZeroVector).Equals(FVector(0.0, 0.0, 1500.0), 1.e-3));

	// The surface's own velocity is added when the params ask for it, and only then
	const FVector Moving(0.0, 200.0, 0.0);
	TestTrue(TEXT("A static configuration ignores the surface velocity"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, Falling, Moving).Equals(FVector(0.0, 0.0, 1500.0), 1.e-3));
	Params.bAddSurfaceVelocity = true;
	TestTrue(TEXT("bAddSurfaceVelocity adds it"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, Falling, Moving).Equals(FVector(0.0, 200.0, 1500.0), 1.e-3));

	// Scaled by the arrival speed into the touched face: 900 in, 0.8 back, clamped to the range
	Params.bAddSurfaceVelocity = false;
	Params.SpeedMode = EGSTN_SurfaceLaunchSpeed::ScaledByIncoming;
	Params.Restitution = 0.8f;
	Params.MinSpeed = 1000.f;
	Params.MaxSpeed = 2000.f;
	TestTrue(TEXT("A soft landing launches at the minimum"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, Falling, FVector::ZeroVector).Equals(FVector(0.0, 0.0, 1000.0), 1.e-3));
	Params.Restitution = 2.f;
	TestTrue(TEXT("The arrival speed scales the launch (900 x 2 = 1800)"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, Falling, FVector::ZeroVector).Equals(FVector(0.0, 0.0, 1800.0), 1.e-3));
	TestTrue(TEXT("A combatant standing on the face arrives at zero and gets the minimum"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Params, Flat, FVector::UpVector, FVector::ZeroVector, FVector::ZeroVector).Equals(FVector(0.0, 0.0, 1000.0), 1.e-3));

	// The move rides the wire with its flags and the velocity
	FGSTN_LayeredMove_Launch Move;
	Move.LaunchVelocity = FVector(100.0, -200.0, 1500.0);
	Move.bResetAirJumps = false;
	Move.bResetCoyote = true;
	FBitWriter Writer(1024, true);
	Move.NetSerialize(Writer);
	FGSTN_LayeredMove_Launch Loaded;
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	Loaded.NetSerialize(Reader);
	TestFalse(TEXT("the wire read cleanly"), Reader.IsError());
	TestTrue(TEXT("the velocity survives the wire"), Loaded.LaunchVelocity.Equals(Move.LaunchVelocity, 0.2));
	TestFalse(TEXT("bResetAirJumps survives"), Loaded.bResetAirJumps);
	TestTrue(TEXT("bResetCoyote survives"), Loaded.bResetCoyote);
	TestTrue(TEXT("A child of Mover's launch, so the tracker reports an external launch"), FGSTN_LayeredMove_Launch::StaticStruct()->IsChildOf(FLayeredMove_Launch::StaticStruct()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LaunchTiltedGravityTest, "GSTNCombat.NP.Hits.Launch.TiltedGravity", GSTN_NP_TEST_FLAGS)
bool FGSTN_LaunchTiltedGravityTest::RunTest(const FString& Parameters)
{
	// A trampoline lying in a world whose gravity is tilted 40 degrees about X: the surface's up is the opposite of that gravity.
	// Nothing in the math reads world Z, so the launch goes along the surface's up and the combatant's tangential part is measured against it
	const FQuat Tilt(FVector::ForwardVector, FMath::DegreesToRadians(40.0));
	const FTransform Surface(Tilt);
	const FVector SurfaceUp = Tilt.GetUpVector();
	const FVector GravityDir = -SurfaceUp;

	FGSTN_SurfaceLaunchParams Params = FixedParams(1200.f);
	const FVector Along = Tilt.GetRightVector() * 250.0;
	const FVector Arriving = GravityDir * 700.0 + Along;
	const FVector Launched = FGSTN_SurfaceLaunch::ComposeVelocity(Params, Surface, SurfaceUp, Arriving, FVector::ZeroVector);
	TestTrue(TEXT("The launch goes along the tilted up with the tangential part kept"), Launched.Equals(SurfaceUp * 1200.0 + Along, 1.e-2));
	TestTrue(TEXT("Not along world up"), FMath::Abs(Launched.GetSafeNormal().Dot(FVector::UpVector)) < 0.999);
	TestEqual(TEXT("The part along the face is what was kept"), Launched.Dot(SurfaceUp), 1200.0, 1.e-2);

	// Tilt mode follows the surface frame too: pitch 30 from the tilted up, towards the surface's forward
	FGSTN_SurfaceLaunchParams TiltMode = FixedParams(1000.f);
	TiltMode.DirectionMode = EGSTN_SurfaceLaunchDirection::Tilt;
	TiltMode.TiltPitch = 30.f;
	TiltMode.TiltYaw = 0.f;
	TiltMode.bKeepTangentialVelocity = false;
	const FVector TiltLaunched = FGSTN_SurfaceLaunch::ComposeVelocity(TiltMode, Surface, SurfaceUp, GravityDir * 500.0, FVector::ZeroVector);
	TestEqual(TEXT("Speed is the configured one"), TiltLaunched.Size(), 1000.0, 1.e-2);
	TestEqual(TEXT("The angle from the surface's up is the pitch"), FMath::RadiansToDegrees(FMath::Acos(TiltLaunched.GetSafeNormal().Dot(SurfaceUp))), 30.0, 1.e-2);

	// A combatant moving away from the face has nothing to scale: no incoming speed
	FGSTN_SurfaceLaunchParams Scaled = FixedParams(0.f);
	Scaled.SpeedMode = EGSTN_SurfaceLaunchSpeed::ScaledByIncoming;
	Scaled.Restitution = 1.f;
	Scaled.MinSpeed = 600.f;
	Scaled.MaxSpeed = 3000.f;
	TestEqual(TEXT("Rising speed does not count as arrival"),
		FGSTN_SurfaceLaunch::ComposeVelocity(Scaled, Surface, SurfaceUp, SurfaceUp * 400.0, FVector::ZeroVector).Dot(SurfaceUp), 600.0, 1.e-2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LaunchResetAirJumpsFlagTest, "GSTNCombat.NP.Hits.Launch.ResetAirJumpsFlag", GSTN_NP_TEST_FLAGS)
bool FGSTN_LaunchResetAirJumpsFlagTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpParams Params;
	Params.AirJumpCount = 2;
	Params.CoyoteFrames = 5;

	FGSTN_JumpState Spent;
	Spent.Phase = EGSTN_JumpPhase::Falling;
	Spent.AirJumpsUsed = 2;
	Spent.CoyoteFramesLeft = 0;

	FGSTN_JumpMoverSample Launched;
	Launched.bGrounded = false;
	Launched.UpSpeed = 1500.0;
	Launched.bExternalLaunch = true;

	// The kit would keep the count and clear the coyote window; the launch overrides both by its flags
	FGSTN_JumpResetRules Kit;
	Kit.AirJumpsOnExternalLaunch = EGSTN_AirJumpsOnLaunch::Keep;
	Kit.CoyoteOnExternalLaunch = EGSTN_CoyoteOnLaunch::Clear;

	const FGSTN_JumpResetRules Refill = FGSTN_SurfaceLaunch::ResetRulesFor(Kit, true, false);
	TestEqual(TEXT("bResetAirJumps refills whatever the kit says"), FGSTN_JumpLogic::Step(Spent, 10, Launched, false, Params, Refill).AirJumpsUsed, static_cast<uint8>(0));

	const FGSTN_JumpResetRules Keep = FGSTN_SurfaceLaunch::ResetRulesFor(Kit, false, false);
	TestEqual(TEXT("Without the flag the count stays"), FGSTN_JumpLogic::Step(Spent, 10, Launched, false, Params, Keep).AirJumpsUsed, static_cast<uint8>(2));

	// The kit that refills is overridden the other way too: a surface can be a launch that gives nothing back
	FGSTN_JumpResetRules RefillingKit;
	RefillingKit.AirJumpsOnExternalLaunch = EGSTN_AirJumpsOnLaunch::Refill;
	TestEqual(TEXT("The flag off beats a kit that refills"),
		FGSTN_JumpLogic::Step(Spent, 10, Launched, false, Params, FGSTN_SurfaceLaunch::ResetRulesFor(RefillingKit, false, false)).AirJumpsUsed, static_cast<uint8>(2));

	// Coyote: the rule stops clearing the window (the combat tick then sets it to its full length), and without the flag the kit's rule stands
	FGSTN_JumpState Coyote = Spent;
	Coyote.CoyoteFramesLeft = 3;
	TestEqual(TEXT("bResetCoyote stops the rule from clearing the window (it ticks down as usual)"),
		FGSTN_JumpLogic::Step(Coyote, 10, Launched, false, Params, FGSTN_SurfaceLaunch::ResetRulesFor(Kit, false, true)).CoyoteFramesLeft, static_cast<uint8>(2));
	TestEqual(TEXT("Without it the kit clears the window"),
		FGSTN_JumpLogic::Step(Coyote, 10, Launched, false, Params, FGSTN_SurfaceLaunch::ResetRulesFor(Kit, false, false)).CoyoteFramesLeft, static_cast<uint8>(0));

	// A frame that is not a launch is not touched by any of it
	FGSTN_JumpMoverSample Plain = Launched;
	Plain.bExternalLaunch = false;
	TestEqual(TEXT("No launch, no reset"), FGSTN_JumpLogic::Step(Spent, 10, Plain, false, Params, Refill).AirJumpsUsed, static_cast<uint8>(2));
	return true;
}

#endif
