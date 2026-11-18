// Pure combat logic: the jump from designer numbers (GSTN_JumpDesigner.h).

#include "Jump/GSTN_JumpDesigner.h"

#include "Jump/GSTN_JumpLogic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_JumpDesigner)

namespace
{
	FGSTN_JumpParams MakeParams(const FGSTN_JumpDesignerTuning& Tuning, double Rise, double Cut, double Fall, int32 CutRampFrames, EGSTN_JumpGravityModel Model)
	{
		const double G = FMath::Max<double>(Tuning.WorldGravity, 1.0);
		FGSTN_JumpParams Params;
		Params.RiseGravityScale = static_cast<float>(Rise / G);
		Params.CutGravityScale = static_cast<float>(Cut / G);
		Params.FallGravityScale = static_cast<float>(Fall / G);
		Params.MinHoldFrames = Tuning.MinHoldFrames;
		Params.MaxHoldFrames = Tuning.MaxHoldFrames;
		Params.CutRampFrames = CutRampFrames;
		Params.GravityModel = Model;
		Params.MaxFallSpeed = 0.f;
		return Params;
	}

	FGSTN_JumpApex Simulate(const FGSTN_JumpDesignerTuning& Tuning, const FGSTN_JumpParams& Params, double JumpVelocity, int32 HoldFrames, double Step)
	{
		return FGSTN_JumpLogic::SimulateApex(FVector::DownVector, FMath::Max<double>(Tuning.WorldGravity, 1.0), JumpVelocity, HoldFrames, Params, Step);
	}
}

void FGSTN_JumpDesigner::SolveClosedForm(const FGSTN_JumpDesignerTuning& Tuning, EGSTN_JumpGravityModel Model, double StepSeconds,
	double& OutJumpVelocity, double& OutRiseGravity, double& OutCutGravity, double& OutFallGravity)
{
	const double H = FMath::Max<double>(Tuning.FullJumpHeight, 1.0);
	const double TApex = FMath::Max<double>(Tuning.TimeToApex, 0.01);
	const double K = FMath::Clamp<double>(Tuning.HoldToFallGravityRatio, 0.01, 1.0);
	const double C = FMath::Max<double>(Tuning.CutToFallGravityRatio, 0.01);
	const double T = FMath::Max(Tuning.MaxHoldFrames, 1) * FMath::Max(StepSeconds, 0.001);
	// After the hold window: the fall gravity (HoldWindow) or the cut gravity until the apex (CutUntilApex)
	const double M = Model == EGSTN_JumpGravityModel::CutUntilApex ? C : 1.0;

	double Fall;
	if (TApex > T)
	{
		const double A = TApex - T;
		Fall = H / (M * A * T + K * T * T * 0.5 + M * A * A * 0.5);
		OutJumpVelocity = Fall * (M * A + K * T);
	}
	else
	{
		const double Rise = 2.0 * H / (TApex * TApex);
		Fall = Rise / K;
		OutJumpVelocity = 2.0 * H / TApex;
	}
	OutFallGravity = Fall;
	OutRiseGravity = K * Fall;
	OutCutGravity = C * Fall;
}

void FGSTN_JumpDesigner::Refresh(FGSTN_JumpDesignerTuning& Tuning, double StepSeconds)
{
	const int32 CutRampFrames = Tuning.CutRampFrames;
	const EGSTN_JumpGravityModel Model = Tuning.GravityModel;
	double V0 = 0.0, Rise = 0.0, Cut = 0.0, Fall = 0.0;
	SolveClosedForm(Tuning, Model, StepSeconds, V0, Rise, Cut, Fall);
	const FGSTN_JumpParams Params = MakeParams(Tuning, Rise, Cut, Fall, CutRampFrames, Model);

	// Secant on the take-off speed so the stepped full jump reaches FullJumpHeight exactly (the gravities keep the closed-form timing)
	constexpr int32 FullHold = 100000;
	const double Target = Tuning.FullJumpHeight;
	double X0 = V0;
	double F0 = Simulate(Tuning, Params, X0, FullHold, StepSeconds).Height - Target;
	double X1 = V0 * 1.03;
	double F1 = Simulate(Tuning, Params, X1, FullHold, StepSeconds).Height - Target;
	for (int32 Iteration = 0; Iteration < 12 && FMath::Abs(F1) > 0.05 && !FMath::IsNearlyEqual(F1, F0); ++Iteration)
	{
		const double X2 = FMath::Max(X1 - F1 * (X1 - X0) / (F1 - F0), 1.0);
		X0 = X1;
		F0 = F1;
		X1 = X2;
		F1 = Simulate(Tuning, Params, X1, FullHold, StepSeconds).Height - Target;
	}

	const FGSTN_JumpApex Full = Simulate(Tuning, Params, X1, FullHold, StepSeconds);
	const FGSTN_JumpApex Short = Simulate(Tuning, Params, X1, 0, StepSeconds);
	Tuning.DerivedJumpVelocity = static_cast<float>(X1);
	Tuning.DerivedRiseGravityScale = Params.RiseGravityScale;
	Tuning.DerivedCutGravityScale = Params.CutGravityScale;
	Tuning.DerivedFallGravityScale = Params.FallGravityScale;
	Tuning.ResultFullHeight = static_cast<float>(Full.Height);
	Tuning.ResultFullApexFrames = Full.Frames;
	Tuning.ResultMinHeight = static_cast<float>(Short.Height);
	Tuning.ResultMinApexFrames = Short.Frames;
	Tuning.bDerivedValid = true;
}

void FGSTN_JumpDesigner::ApplyToParams(const FGSTN_JumpDesignerTuning& Tuning, FGSTN_JumpParams& InOutParams)
{
	if (!Tuning.bEnabled || !Tuning.bDerivedValid)
	{
		return;
	}
	InOutParams.RiseGravityScale = Tuning.DerivedRiseGravityScale;
	InOutParams.CutGravityScale = Tuning.DerivedCutGravityScale;
	InOutParams.FallGravityScale = Tuning.DerivedFallGravityScale;
	InOutParams.MinHoldFrames = Tuning.MinHoldFrames;
	InOutParams.MaxHoldFrames = Tuning.MaxHoldFrames;
	InOutParams.CutRampFrames = Tuning.CutRampFrames;
	InOutParams.GravityModel = Tuning.GravityModel;
}
