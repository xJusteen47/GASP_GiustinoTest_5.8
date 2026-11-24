// Automation tests of the greybox capsule and the simple locomotion graph (greybox-capsula, 2026-11-24). Pure: no world.

#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_GreyboxAnimInstance.h"
#include "Presentation/GSTN_SimpleLocoAnimInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_GREYBOX_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SimpleLocoStatesTest, "GSTNCombat.NP.Greybox.SimpleLocoStates", GSTN_GREYBOX_TEST_FLAGS)
bool FGSTN_SimpleLocoStatesTest::RunTest(const FString& Parameters)
{
	using EState = EGSTN_SimpleLocoState;
	const FGSTN_SimpleLocoSettings Settings;
	FGSTN_SimpleLocoInputs Still;
	FGSTN_SimpleLocoInputs Moving;
	Moving.Speed2D = 300.f;
	Moving.StickAlong = 1.f;

	TestTrue(TEXT("still on the ground = Idle"), FGSTN_SimpleLocoLogic::NextState(Still, EState::Run, 1.f, Settings) == EState::Idle);
	TestTrue(TEXT("moving from Idle = RunStart"), FGSTN_SimpleLocoLogic::NextState(Moving, EState::Idle, 1.f, Settings) == EState::RunStart);
	TestTrue(TEXT("RunStart holds for its window"), FGSTN_SimpleLocoLogic::NextState(Moving, EState::RunStart, 0.1f, Settings) == EState::RunStart);
	TestTrue(TEXT("then Run"), FGSTN_SimpleLocoLogic::NextState(Moving, EState::RunStart, Settings.RunStartSeconds + 0.01f, Settings) == EState::Run);
	TestTrue(TEXT("moving out of the air = Run, not a start"), FGSTN_SimpleLocoLogic::NextState(Moving, EState::Air, 0.5f, Settings) == EState::Run);

	FGSTN_SimpleLocoInputs Released = Moving;
	Released.StickAlong = 0.f;
	TestTrue(TEXT("stick released while running = RunStop"), FGSTN_SimpleLocoLogic::NextState(Released, EState::Run, 1.f, Settings) == EState::RunStop);
	TestTrue(TEXT("RunStop until still"), FGSTN_SimpleLocoLogic::NextState(Released, EState::RunStop, 0.2f, Settings) == EState::RunStop);
	TestTrue(TEXT("stick pushed again ends the stop"), FGSTN_SimpleLocoLogic::NextState(Moving, EState::RunStop, 0.2f, Settings) == EState::Run);

	FGSTN_SimpleLocoInputs Air = Moving;
	Air.bOnGround = false;
	Air.bSkidding = true;
	TestTrue(TEXT("air wins over everything"), FGSTN_SimpleLocoLogic::NextState(Air, EState::Run, 1.f, Settings) == EState::Air);

	FGSTN_SimpleLocoInputs Skid = Moving;
	Skid.bSkidding = true;
	TestTrue(TEXT("skid on the ground"), FGSTN_SimpleLocoLogic::NextState(Skid, EState::Run, 1.f, Settings) == EState::Skid);

	FGSTN_SimpleLocoInputs Crouch = Still;
	Crouch.bCrouching = true;
	TestTrue(TEXT("crouch"), FGSTN_SimpleLocoLogic::NextState(Crouch, EState::Idle, 1.f, Settings) == EState::Crouch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GreyboxShapeTest, "GSTNCombat.NP.Greybox.Shape", GSTN_GREYBOX_TEST_FLAGS)
bool FGSTN_GreyboxShapeTest::RunTest(const FString& Parameters)
{
	const FGSTN_GreyboxShapeSettings Settings;
	TestTrue(TEXT("rest on the ground = 1"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::SquashTarget(Settings, true, 0.f, 0.f), 1.f));
	TestTrue(TEXT("stretch in the air"), UGSTN_GreyboxAnimInstance::SquashTarget(Settings, false, -900.f, 0.f) > 1.05f);
	TestTrue(TEXT("never past max stretch"), UGSTN_GreyboxAnimInstance::SquashTarget(Settings, false, -90000.f, 0.f) <= Settings.MaxStretch);
	TestTrue(TEXT("a speed gain squashes"), UGSTN_GreyboxAnimInstance::SquashTarget(Settings, true, 0.f, 3000.f) < 1.f);
	TestTrue(TEXT("braking does not squash"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::SquashTarget(Settings, true, 0.f, -3000.f), 1.f));
	TestTrue(TEXT("never under min squash"), UGSTN_GreyboxAnimInstance::SquashTarget(Settings, true, 0.f, 1.e6f) >= Settings.MinSquash);

	const FRotator Forward = UGSTN_GreyboxAnimInstance::LeanTarget(Settings, FVector(2000.f, 0.f, 0.f));
	TestTrue(TEXT("accelerating forward leans forward (negative pitch)"), Forward.Pitch < -1.f && FMath::IsNearlyZero(Forward.Roll));
	const FRotator Right = UGSTN_GreyboxAnimInstance::LeanTarget(Settings, FVector(0.f, 2000.f, 0.f));
	TestTrue(TEXT("accelerating right leans right"), Right.Roll > 1.f);
	const FRotator Hard = UGSTN_GreyboxAnimInstance::LeanTarget(Settings, FVector(1.e6f, 1.e6f, 0.f));
	TestTrue(TEXT("forward lean capped"), FMath::Abs(Hard.Pitch) <= Settings.MaxLeanDeg + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("curve lean capped"), FMath::Abs(Hard.Roll) <= Settings.MaxTurnLeanDeg + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("a curve leans harder than a start"), Right.Roll > -Forward.Pitch);
	TestTrue(TEXT("hard braking from a run starts a run stop"), UGSTN_GreyboxAnimInstance::StartsRunStop(Settings, true, 500.f, -3000.f, false));
	TestFalse(TEXT("only once per stop"), UGSTN_GreyboxAnimInstance::StartsRunStop(Settings, true, 500.f, -3000.f, true));
	TestFalse(TEXT("a walk does not kick"), UGSTN_GreyboxAnimInstance::StartsRunStop(Settings, true, 100.f, -3000.f, false));
	TestFalse(TEXT("gentle slowing does not kick"), UGSTN_GreyboxAnimInstance::StartsRunStop(Settings, true, 500.f, -300.f, false));

	TestTrue(TEXT("pelvis at reference = 1"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::PelvisSquashTarget(Settings, true, 95.f, 95.f), 1.f));
	TestTrue(TEXT("crouched pelvis squashes"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::PelvisSquashTarget(Settings, true, 57.f, 95.f), 0.6f, 1.e-3f));
	TestTrue(TEXT("pelvis squash floor"), UGSTN_GreyboxAnimInstance::PelvisSquashTarget(Settings, true, 0.f, 95.f) >= Settings.MinPelvisSquash);
	TestTrue(TEXT("raised pelvis does not stretch"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::PelvisSquashTarget(Settings, true, 120.f, 95.f), 1.f));
	TestTrue(TEXT("limbs hidden in locomotion"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::LimbScaleTarget(Settings, 0.f)));
	TestTrue(TEXT("limbs shown in a move"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::LimbScaleTarget(Settings, 1.f), 1.f));
	TestTrue(TEXT("frantic right after leaving rest"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::FranticAlpha(Settings, 0.1f), 1.f));
	TestTrue(TEXT("frantic over after its window"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::FranticAlpha(Settings,
		Settings.FranticSeconds + Settings.FranticFadeSeconds + 0.01f)));
	TestTrue(TEXT("not frantic at rest"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::FranticAlpha(Settings, -1.f)));
	// Run 3 (owner: the run start rocks too wide): at full speed inside the frantic window the amplitude is FranticAmplitude, not the run's
	TestTrue(TEXT("frantic amplitude replaces the speed's"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::SwayAmplitudeTarget(Settings,
		Settings.FullSwaySpeed * 2.f, 1.f), Settings.FranticAmplitude));
	TestTrue(TEXT("speed amplitude after the frantic start"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::SwayAmplitudeTarget(Settings,
		Settings.FullSwaySpeed, 0.f), 1.f));
	TestTrue(TEXT("run start cadence faster than the speed's"), UGSTN_GreyboxAnimInstance::SwayCadenceHz(Settings, 150.f, 1.f)
		> 2.f * UGSTN_GreyboxAnimInstance::SwayCadenceHz(Settings, 150.f, 0.f));
	TestTrue(TEXT("still = no cadence"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::SwayCadenceHz(Settings, 0.f, 0.f)));
	float NodPitch = 0.f;
	const FVector AtFootfall = UGSTN_GreyboxAnimInstance::RunSway(Settings, 0.f, 1.f, NodPitch);
	TestTrue(TEXT("footfall bobs down and nods forward"), AtFootfall.X < 0.f && NodPitch < 0.f);
	TestTrue(TEXT("no amplitude = no sway"), UGSTN_GreyboxAnimInstance::RunSway(Settings, 1.f, 0.f, NodPitch).IsNearlyZero());
	// Owner 14:35: a hard brake tips the top against the motion (the leg on the motion's side goes in front)
	const FRotator BrakeForward = UGSTN_GreyboxAnimInstance::PlantLean(FVector(1.f, 0.f, 0.f), 20.f);
	TestTrue(TEXT("forward brake tips back"), FMath::IsNearlyEqual(BrakeForward.Pitch, 20.f) && FMath::IsNearlyZero(BrakeForward.Roll));
	const FRotator BrakeRight = UGSTN_GreyboxAnimInstance::PlantLean(FVector(0.f, 1.f, 0.f), 20.f);
	TestTrue(TEXT("brake of a motion to the right tips left"), FMath::IsNearlyEqual(BrakeRight.Roll, -20.f) && FMath::IsNearlyZero(BrakeRight.Pitch));
	// Owner 15:26 (wall brake = friction, not a sway): the stick-slip builds slowly and snaps back fast
	TestTrue(TEXT("stick-slip starts low"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::StickSlip(0.f), -0.5f));
	TestTrue(TEXT("stick-slip peaks at the slip"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::StickSlip(0.8f), 0.5f));
	const float CatchRate = UGSTN_GreyboxAnimInstance::StickSlip(0.1f) - UGSTN_GreyboxAnimInstance::StickSlip(0.f);
	const float SlipRate = UGSTN_GreyboxAnimInstance::StickSlip(0.8f) - UGSTN_GreyboxAnimInstance::StickSlip(0.9f);
	TestTrue(TEXT("stick-slip catch is slower than the slip"), CatchRate > 0.f && SlipRate > 2.f * CatchRate);
	TestTrue(TEXT("no motion = no kick"), UGSTN_GreyboxAnimInstance::PlantLean(FVector::ZeroVector, 20.f).IsNearlyZero());
	TestTrue(TEXT("no pelvis squash in the air"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::PelvisSquashTarget(Settings, false, 40.f, 95.f), 1.f));
	// Owner 2026-11-24 (the capsule squashes on the ground instead of turning): a tipped pelvis lies the capsule down
	TestTrue(TEXT("upright pelvis does not lie"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::LyingTarget(Settings, 1.f)));
	TestTrue(TEXT("a crouch's small tilt does not lie"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::LyingTarget(Settings,
		FMath::Cos(FMath::DegreesToRadians(Settings.LyingStartDeg - 5.f)))));
	TestTrue(TEXT("flat pelvis lies fully"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::LyingTarget(Settings, 0.f), 1.f));
	TestTrue(TEXT("upside down (a somersault) lies fully"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::LyingTarget(Settings, -0.8f), 1.f));
	const float Halfway = FMath::Cos(FMath::DegreesToRadians(0.5f * (Settings.LyingStartDeg + Settings.LyingFullDeg)));
	TestTrue(TEXT("lying blends in between"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::LyingTarget(Settings, Halfway), 0.5f, 1.e-3f));
	FGSTN_GreyboxShapeSettings Off;
	Off.bLyingFollowsPelvis = false;
	TestTrue(TEXT("switch off = never lies"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::LyingTarget(Off, 0.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GreyboxSpringTest, "GSTNCombat.NP.Greybox.Spring", GSTN_GREYBOX_TEST_FLAGS)
bool FGSTN_GreyboxSpringTest::RunTest(const FString& Parameters)
{
	FGSTN_GreyboxSpring Spring;
	float Peak = 0.f;
	for (int32 Frame = 0; Frame < 240; ++Frame)
	{
		Spring.Step(1.f, 1.f / 60.f, 5.f, 0.45f);
		Peak = FMath::Max(Peak, Spring.Value);
	}
	TestTrue(TEXT("an underdamped spring overshoots once"), Peak > 1.05f);
	TestTrue(TEXT("and settles on the target"), FMath::IsNearlyEqual(Spring.Value, 1.f, 0.01f));

	FGSTN_GreyboxSpring Hitch;
	Hitch.Step(1.f, 0.5f, 5.f, 0.45f);
	TestTrue(TEXT("a long frame stays bounded"), FMath::Abs(Hitch.Value) < 3.f);
	return true;
}

#endif
