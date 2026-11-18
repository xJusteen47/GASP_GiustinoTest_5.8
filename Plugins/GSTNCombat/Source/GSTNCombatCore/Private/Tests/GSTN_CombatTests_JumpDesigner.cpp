// Headless tests of the jump designer (owner 2026-11-18): the closed form with the hold window and the gravity change, and the heights the
// real state machine reaches at the sim rate (62.5 Hz).

#include "Tests/GSTN_CombatTestUtils.h"
#include "Jump/GSTN_JumpDesigner.h"
#include "Jump/GSTN_JumpLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double SimStep = 0.016;   // 62.5 Hz

	FGSTN_JumpParams ParamsOf(const FGSTN_JumpDesignerTuning& Tuning)
	{
		FGSTN_JumpParams Params;
		FGSTN_JumpDesignerTuning On = Tuning;
		On.bEnabled = true;
		FGSTN_JumpDesigner::ApplyToParams(On, Params);
		Params.MaxFallSpeed = 0.f;
		return Params;
	}

	/** Full and short hop measured independently of Refresh's own measurement */
	FGSTN_JumpApex Measure(const FGSTN_JumpDesignerTuning& Tuning, int32 HoldFrames, double Step = SimStep)
	{
		return FGSTN_JumpLogic::SimulateApex(FVector::DownVector, Tuning.WorldGravity, Tuning.DerivedJumpVelocity, HoldFrames, ParamsOf(Tuning), Step);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpDesignerWindowTest, "GSTNCombat.Core.JumpDesigner.HoldWindowEndsBeforeApex", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpDesignerWindowTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpDesignerTuning Tuning;   // 350 cm in 0.5 s, window 30 frames (0.48 s), hold gravity 0.29 x fall
	FGSTN_JumpDesigner::Refresh(Tuning, SimStep);
	TestTrue(TEXT("derived"), Tuning.bDerivedValid && Tuning.DerivedJumpVelocity > 0.f && Tuning.DerivedFallGravityScale > Tuning.DerivedRiseGravityScale);

	const FGSTN_JumpApex Full = Measure(Tuning, 100000);
	TestTrue(FString::Printf(TEXT("full jump %.1f cm = 350 at 62.5 Hz"), Full.Height), FMath::IsNearlyEqual(Full.Height, 350.0, 0.5));
	TestTrue(FString::Printf(TEXT("apex at frame %d ~ 0.5 s (31 frames)"), Full.Frames), FMath::Abs(Full.Frames - 31) <= 2);
	TestTrue(TEXT("the shown full height is the measured one"), FMath::IsNearlyEqual(Tuning.ResultFullHeight, Full.Height, 0.01));

	const FGSTN_JumpApex Short = Measure(Tuning, 0);
	TestTrue(FString::Printf(TEXT("short hop %.1f cm lower than the full jump"), Short.Height), Short.Height > 10.0 && Short.Height < Full.Height * 0.8);
	TestTrue(TEXT("the shown short hop is the measured one"), FMath::IsNearlyEqual(Tuning.ResultMinHeight, Short.Height, 0.01));

	// A longer minimum hold raises the short hop, never the full jump
	FGSTN_JumpDesignerTuning MinHold = Tuning;
	MinHold.MinHoldFrames = 8;
	FGSTN_JumpDesigner::Refresh(MinHold, SimStep);
	TestTrue(TEXT("min hold raises the short hop"), MinHold.ResultMinHeight > Tuning.ResultMinHeight);
	TestTrue(TEXT("min hold keeps the full jump"), FMath::IsNearlyEqual(MinHold.ResultFullHeight, 350.f, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpDesignerInsideTest, "GSTNCombat.Core.JumpDesigner.ApexInsideHold", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpDesignerInsideTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpDesignerTuning Tuning;
	Tuning.FullJumpHeight = 200.f;
	Tuning.TimeToApex = 0.3f;   // inside the 0.48 s window
	FGSTN_JumpDesigner::Refresh(Tuning, SimStep);
	const FGSTN_JumpApex Full = Measure(Tuning, 100000);
	TestTrue(FString::Printf(TEXT("full jump %.1f cm = 200"), Full.Height), FMath::IsNearlyEqual(Full.Height, 200.0, 0.5));
	TestTrue(FString::Printf(TEXT("apex at frame %d ~ 19"), Full.Frames), FMath::Abs(Full.Frames - 19) <= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpDesignerClosedFormTest, "GSTNCombat.Core.JumpDesigner.ClosedForm", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpDesignerClosedFormTest::RunTest(const FString& Parameters)
{
	// The closed form is the continuous jump: a very fine step with no cut ramp lands on the inputs (both gravity models)
	for (const EGSTN_JumpGravityModel Model : { EGSTN_JumpGravityModel::HoldWindow, EGSTN_JumpGravityModel::CutUntilApex })
	{
		constexpr double Fine = 0.0005;
		FGSTN_JumpDesignerTuning Tuning;
		Tuning.GravityModel = Model;
		Tuning.CutRampFrames = 0;
		Tuning.CutToFallGravityRatio = 1.5f;
		Tuning.MaxHoldFrames = 960;   // 0.48 s at the fine step
		double V0 = 0.0, Rise = 0.0, Cut = 0.0, Fall = 0.0;
		FGSTN_JumpDesigner::SolveClosedForm(Tuning, Model, Fine, V0, Rise, Cut, Fall);
		TestTrue(TEXT("hold over fall ratio kept"), FMath::IsNearlyEqual(Rise / Fall, 0.29, 1e-4));
		TestTrue(TEXT("cut over fall ratio kept"), FMath::IsNearlyEqual(Cut / Fall, 1.5, 1e-4));

		FGSTN_JumpParams Params;
		Params.RiseGravityScale = static_cast<float>(Rise / Tuning.WorldGravity);
		Params.CutGravityScale = static_cast<float>(Cut / Tuning.WorldGravity);
		Params.FallGravityScale = static_cast<float>(Fall / Tuning.WorldGravity);
		Params.MinHoldFrames = 0;
		Params.MaxHoldFrames = Tuning.MaxHoldFrames;
		Params.CutRampFrames = 0;
		Params.GravityModel = Model;
		const FGSTN_JumpApex Apex = FGSTN_JumpLogic::SimulateApex(FVector::DownVector, Tuning.WorldGravity, V0, 100000, Params, Fine);
		TestTrue(FString::Printf(TEXT("model %d: continuous height %.2f ~ 350"), static_cast<int32>(Model), Apex.Height), FMath::IsNearlyEqual(Apex.Height, 350.0, 2.0));
		TestTrue(FString::Printf(TEXT("model %d: continuous apex %.3f s ~ 0.5"), static_cast<int32>(Model), Apex.Frames * Fine), FMath::IsNearlyEqual(Apex.Frames * Fine, 0.5, 0.005));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpDesignerApplyTest, "GSTNCombat.Core.JumpDesigner.Apply", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpDesignerApplyTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpDesignerTuning Tuning;
	FGSTN_JumpDesigner::Refresh(Tuning, SimStep);
	FGSTN_JumpParams Stats;
	Stats.RiseGravityScale = 9.f;
	FGSTN_JumpDesigner::ApplyToParams(Tuning, Stats);
	TestTrue(TEXT("off: the stats stay"), Stats.RiseGravityScale == 9.f);
	Tuning.bEnabled = true;
	FGSTN_JumpDesigner::ApplyToParams(Tuning, Stats);
	TestTrue(TEXT("on: the derived scales replace them"), Stats.RiseGravityScale == Tuning.DerivedRiseGravityScale && Stats.MaxHoldFrames == Tuning.MaxHoldFrames);
	return true;
}

#endif
