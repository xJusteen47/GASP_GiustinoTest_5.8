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
	TestTrue(TEXT("lean capped"), FVector2D(Hard.Pitch, Hard.Roll).Size() <= Settings.MaxLeanDeg + KINDA_SMALL_NUMBER);

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
	TestTrue(TEXT("run start cadence faster than the speed's"), UGSTN_GreyboxAnimInstance::SwayCadenceHz(Settings, 150.f, 1.f)
		> 2.f * UGSTN_GreyboxAnimInstance::SwayCadenceHz(Settings, 150.f, 0.f));
	TestTrue(TEXT("still = no cadence"), FMath::IsNearlyZero(UGSTN_GreyboxAnimInstance::SwayCadenceHz(Settings, 0.f, 0.f)));
	float NodPitch = 0.f;
	const FVector AtFootfall = UGSTN_GreyboxAnimInstance::RunSway(Settings, 0.f, 1.f, NodPitch);
	TestTrue(TEXT("footfall bobs down and nods forward"), AtFootfall.X < 0.f && NodPitch < 0.f);
	TestTrue(TEXT("no amplitude = no sway"), UGSTN_GreyboxAnimInstance::RunSway(Settings, 1.f, 0.f, NodPitch).IsNearlyZero());
	TestTrue(TEXT("no pelvis squash in the air"), FMath::IsNearlyEqual(UGSTN_GreyboxAnimInstance::PelvisSquashTarget(Settings, false, 40.f, 95.f), 1.f));
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
