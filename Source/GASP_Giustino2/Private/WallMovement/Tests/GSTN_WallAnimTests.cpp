// Tests of the wall anim state (wall-anim.md): GSTN.Wall.Anim.*
//
// Pure: no world, no Mover component. They pin the velocity split, the loop classification with its hysteresis and the phase steps.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WallMovement/Animation/GSTN_WallAnimUtils.h"

namespace GSTNWallAnimTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Wall in front of a character facing +X: normal -X. With Z up, the character's right is +Y */
	const FVector WallNormal(-1.f, 0.f, 0.f);
	const FVector Up(0.f, 0.f, 1.f);

	static FGSTN_WallAnimInputs OnWall(const FVector& Velocity, bool bClimb = false)
	{
		FGSTN_WallAnimInputs Inputs;
		Inputs.bOnWall = true;
		Inputs.bIsClimbing = bClimb;
		Inputs.ModeName = bClimb ? FName(TEXT("WallClimbingV2")) : FName(TEXT("WallSlidingV2"));
		Inputs.Velocity = Velocity;
		Inputs.WallNormal = WallNormal;
		Inputs.GravityUp = Up;
		return Inputs;
	}

	static FGSTN_WallAnimInputs OffWall(FName Mode)
	{
		FGSTN_WallAnimInputs Inputs;
		Inputs.ModeName = Mode;
		Inputs.GravityUp = Up;
		return Inputs;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallAnimSplitTest, "GSTN.Wall.Anim.SplitWallVelocity", GSTNWallAnimTest::TestFlags)
bool FGSTN_WallAnimSplitTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallAnimTest;
	float Lateral = 0.f, Vertical = 0.f, PlaneSpeed = 0.f;

	// Right of the character, down the wall, and an into-wall part that must not count
	UGSTN_WallAnimUtils::SplitWallVelocity(FVector(500.f, 200.f, -300.f), WallNormal, Up, Lateral, Vertical, PlaneSpeed);
	TestTrue(TEXT("Lateral is +Y (right)"), FMath::IsNearlyEqual(Lateral, 200.f, 0.01f));
	TestTrue(TEXT("Vertical is up positive"), FMath::IsNearlyEqual(Vertical, -300.f, 0.01f));
	TestTrue(TEXT("Into-wall part ignored"), FMath::IsNearlyEqual(PlaneSpeed, FMath::Sqrt(200.f * 200.f + 300.f * 300.f), 0.01f));

	// Gravity-up along +Y: the same wall's right is -Z and a +Y velocity is vertical
	UGSTN_WallAnimUtils::SplitWallVelocity(FVector(0.f, 100.f, 0.f), WallNormal, FVector(0.f, 1.f, 0.f), Lateral, Vertical, PlaneSpeed);
	TestTrue(TEXT("Any gravity: vertical follows gravity-up"), FMath::IsNearlyEqual(Vertical, 100.f, 0.01f) && FMath::IsNearlyZero(Lateral, 0.01f));

	// A floor has no wall axes
	UGSTN_WallAnimUtils::SplitWallVelocity(FVector(100.f, 0.f, 0.f), Up, Up, Lateral, Vertical, PlaneSpeed);
	TestTrue(TEXT("Floor: no lateral, no vertical"), FMath::IsNearlyZero(Lateral) && FMath::IsNearlyZero(Vertical));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallAnimClassifyTest, "GSTN.Wall.Anim.ClassifySlideHysteresis", GSTNWallAnimTest::TestFlags)
bool FGSTN_WallAnimClassifyTest::RunTest(const FString& Parameters)
{
	const FGSTN_WallAnimSettings S; // Skid 750/650, lateral 120/80
	using EState = EGSTN_WallSlideAnimState;

	TestTrue(TEXT("Still -> Idle"), UGSTN_WallAnimUtils::ClassifySlide(EState::Idle, 0.f, 0.f, false, S) == EState::Idle);
	TestTrue(TEXT("Straight down at terminal speed -> Idle"), UGSTN_WallAnimUtils::ClassifySlide(EState::Idle, 0.f, 600.f, false, S) == EState::Idle);
	TestTrue(TEXT("Right"), UGSTN_WallAnimUtils::ClassifySlide(EState::Idle, 150.f, 150.f, false, S) == EState::SlideRight);
	TestTrue(TEXT("Left"), UGSTN_WallAnimUtils::ClassifySlide(EState::Idle, -150.f, 150.f, false, S) == EState::SlideLeft);
	TestTrue(TEXT("Under enter: stays Idle"), UGSTN_WallAnimUtils::ClassifySlide(EState::Idle, 100.f, 100.f, false, S) == EState::Idle);
	TestTrue(TEXT("Hysteresis keeps Right above exit"), UGSTN_WallAnimUtils::ClassifySlide(EState::SlideRight, 100.f, 100.f, false, S) == EState::SlideRight);
	TestTrue(TEXT("Below exit -> Idle"), UGSTN_WallAnimUtils::ClassifySlide(EState::SlideRight, 70.f, 70.f, false, S) == EState::Idle);
	TestTrue(TEXT("Sign flip needs the enter speed"), UGSTN_WallAnimUtils::ClassifySlide(EState::SlideRight, -100.f, 100.f, false, S) == EState::Idle);
	TestTrue(TEXT("Fast -> Skid wins over a side"), UGSTN_WallAnimUtils::ClassifySlide(EState::SlideRight, 300.f, 800.f, false, S) == EState::Skid);
	TestTrue(TEXT("Skid kept above exit"), UGSTN_WallAnimUtils::ClassifySlide(EState::Skid, 0.f, 700.f, false, S) == EState::Skid);
	TestTrue(TEXT("Skid ends below exit"), UGSTN_WallAnimUtils::ClassifySlide(EState::Skid, 0.f, 600.f, false, S) == EState::Idle);
	TestTrue(TEXT("Climb never skids by default"), UGSTN_WallAnimUtils::ClassifySlide(EState::Idle, 0.f, 900.f, true, S) == EState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallAnimPhaseTest, "GSTN.Wall.Anim.PhaseSteps", GSTNWallAnimTest::TestFlags)
bool FGSTN_WallAnimPhaseTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallAnimTest;
	FGSTN_WallAnimSettings S;
	S.EnterDuration = 0.2f;
	S.ExitDuration = 0.3f;
	constexpr float Dt = 0.1f;

	FGSTN_WallAnimState State;
	TestTrue(TEXT("Starts None"), State.Phase == EGSTN_WallAnimPhase::None);

	State = UGSTN_WallAnimUtils::StepWallAnimState(State, OnWall(FVector(0.f, 200.f, 0.f)), S, Dt);
	TestTrue(TEXT("Grab -> Enter"), State.Phase == EGSTN_WallAnimPhase::Enter);
	TestTrue(TEXT("Active flag"), State.bIsActive);
	TestTrue(TEXT("Loop state is known during Enter"), State.SlideState == EGSTN_WallSlideAnimState::SlideRight);

	State = UGSTN_WallAnimUtils::StepWallAnimState(State, OnWall(FVector(0.f, 200.f, 0.f)), S, Dt);
	TestTrue(TEXT("Still Enter at 0.1 s"), State.Phase == EGSTN_WallAnimPhase::Enter);
	State = UGSTN_WallAnimUtils::StepWallAnimState(State, OnWall(FVector(0.f, 200.f, 0.f)), S, Dt);
	TestTrue(TEXT("Active at 0.2 s"), State.Phase == EGSTN_WallAnimPhase::Active);

	// Slide <-> climb in place is not an exit
	State = UGSTN_WallAnimUtils::StepWallAnimState(State, OnWall(FVector::ZeroVector, true), S, Dt);
	TestTrue(TEXT("Switch to climb stays Active"), State.Phase == EGSTN_WallAnimPhase::Active);
	TestTrue(TEXT("Climbing flag"), State.bIsClimbing);

	State = UGSTN_WallAnimUtils::StepWallAnimState(State, OffWall(TEXT("Falling")), S, Dt);
	TestTrue(TEXT("Leave -> Exit"), State.Phase == EGSTN_WallAnimPhase::Exit);
	TestTrue(TEXT("Exit mode recorded"), State.ExitModeName == FName(TEXT("Falling")));
	TestTrue(TEXT("Climbing kept during Exit"), State.bIsClimbing);

	// Grab again during the exit: a new entry
	FGSTN_WallAnimState Regrab = UGSTN_WallAnimUtils::StepWallAnimState(State, OnWall(FVector::ZeroVector), S, Dt);
	TestTrue(TEXT("Regrab during Exit -> Enter"), Regrab.Phase == EGSTN_WallAnimPhase::Enter);

	for (int32 Frame = 0; Frame < 4; ++Frame) // one spare frame against float rounding
	{
		State = UGSTN_WallAnimUtils::StepWallAnimState(State, OffWall(TEXT("Falling")), S, Dt);
	}
	TestTrue(TEXT("Exit ends -> None"), State.Phase == EGSTN_WallAnimPhase::None);
	TestFalse(TEXT("Not active"), State.bIsActive);

	// Zero durations skip the phases
	S.EnterDuration = 0.f;
	S.ExitDuration = 0.f;
	State = UGSTN_WallAnimUtils::StepWallAnimState(FGSTN_WallAnimState(), OnWall(FVector::ZeroVector), S, Dt);
	TestTrue(TEXT("No enter phase"), State.Phase == EGSTN_WallAnimPhase::Active);
	State = UGSTN_WallAnimUtils::StepWallAnimState(State, OffWall(TEXT("Walking")), S, Dt);
	TestTrue(TEXT("No exit phase"), State.Phase == EGSTN_WallAnimPhase::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallAnimLimbWeightsTest, "GSTN.Wall.Anim.LimbContactWeights", GSTNWallAnimTest::TestFlags)
bool FGSTN_WallAnimLimbWeightsTest::RunTest(const FString& Parameters)
{
	FGSTN_WallLimbContactSettings S;
	S.SlideHandWeight = 0.5f;
	S.SlideFootWeight = 1.f;
	S.ClimbHandWeight = 1.f;
	S.ClimbFootWeight = 0.25f;

	FGSTN_WallAnimState Anim;
	float Hand = -1.f, Foot = -1.f;

	UGSTN_WallAnimUtils::ComputeLimbWeights(Anim, S, Hand, Foot);
	TestTrue(TEXT("Off the wall: no contact"), FMath::IsNearlyZero(Hand) && FMath::IsNearlyZero(Foot));

	Anim.Phase = EGSTN_WallAnimPhase::Enter;
	UGSTN_WallAnimUtils::ComputeLimbWeights(Anim, S, Hand, Foot);
	TestTrue(TEXT("Enter uses the slide weights"), FMath::IsNearlyEqual(Hand, 0.5f) && FMath::IsNearlyEqual(Foot, 1.f));

	Anim.Phase = EGSTN_WallAnimPhase::Active;
	Anim.bIsClimbing = true;
	UGSTN_WallAnimUtils::ComputeLimbWeights(Anim, S, Hand, Foot);
	TestTrue(TEXT("Climbing uses the climb weights"), FMath::IsNearlyEqual(Hand, 1.f) && FMath::IsNearlyEqual(Foot, 0.25f));

	Anim.Phase = EGSTN_WallAnimPhase::Exit;
	UGSTN_WallAnimUtils::ComputeLimbWeights(Anim, S, Hand, Foot);
	TestTrue(TEXT("Exit lets go by default"), FMath::IsNearlyZero(Hand) && FMath::IsNearlyZero(Foot));

	S.bKeepDuringExit = true;
	UGSTN_WallAnimUtils::ComputeLimbWeights(Anim, S, Hand, Foot);
	TestTrue(TEXT("bKeepDuringExit keeps them"), FMath::IsNearlyEqual(Hand, 1.f) && FMath::IsNearlyEqual(Foot, 0.25f));

	S.bEnabled = false;
	UGSTN_WallAnimUtils::ComputeLimbWeights(Anim, S, Hand, Foot);
	TestTrue(TEXT("Disabled: no contact"), FMath::IsNearlyZero(Hand) && FMath::IsNearlyZero(Foot));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
