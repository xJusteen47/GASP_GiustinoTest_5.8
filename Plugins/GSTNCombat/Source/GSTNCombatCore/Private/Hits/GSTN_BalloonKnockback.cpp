// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_BalloonKnockback.h"

double FGSTN_BalloonMath::InitialMultiplier(int32 MinHitstunFrames, int32 FullHitstunFrames, double MaxSpeedMultiplier, int32 FlightFrames)
{
	const int32 Low = FMath::Max(MinHitstunFrames, 1);
	const int32 High = FMath::Max(FullHitstunFrames, Low + 1);
	if (FlightFrames <= Low)
	{
		return 1.0;
	}
	const double Alpha = FMath::Clamp(static_cast<double>(FlightFrames - Low) / static_cast<double>(High - Low), 0.0, 1.0);
	return 1.0 + (FMath::Max(MaxSpeedMultiplier, 1.0) - 1.0) * Alpha;
}

double FGSTN_BalloonMath::InitialMultiplier(const FGSTN_BalloonKnockbackSettings& Settings, int32 FlightFrames)
{
	return InitialMultiplier(Settings.MinHitstunFrames, Settings.FullHitstunFrames, Settings.MaxSpeedMultiplier, FlightFrames);
}

double FGSTN_BalloonMath::Multiplier(double M0, double EndFraction, int32 FlightFrames, double Consumed)
{
	if (M0 <= 1.0 || FlightFrames <= 0)
	{
		return 1.0;
	}
	const double End = FMath::Clamp(EndFraction, 0.0, 0.95);
	const double Left = (static_cast<double>(FlightFrames) - Consumed) / static_cast<double>(FlightFrames);
	return FMath::Max(1.0, M0 * (Left - End) / (1.0 - End));
}

void FGSTN_BalloonMath::Evaluate(double M0, double EndFraction, int32 FlightFrames, int32 FrameIndex, double& OutConsumed, double& OutMultiplier)
{
	double Consumed = 0.0;
	double M = Multiplier(M0, EndFraction, FlightFrames, Consumed);
	// Past the warp M is 1 every frame, so the clock just counts: the loop only walks the warped frames
	const int32 Steps = FMath::Max(FrameIndex, 0);
	int32 Step = 0;
	for (; Step < Steps && M > 1.0; ++Step)
	{
		Consumed += M;
		M = Multiplier(M0, EndFraction, FlightFrames, Consumed);
	}
	Consumed += static_cast<double>(Steps - Step);
	OutConsumed = Consumed;
	OutMultiplier = Steps > Step ? 1.0 : M;
}

int32 FGSTN_BalloonMath::GameFrames(double M0, double EndFraction, int32 FlightFrames)
{
	if (FlightFrames <= 0)
	{
		return 0;
	}
	double Consumed = 0.0;
	int32 Frames = 0;
	while (Consumed < static_cast<double>(FlightFrames) && Frames < FlightFrames)
	{
		Consumed += Multiplier(M0, EndFraction, FlightFrames, Consumed);
		++Frames;
	}
	return Frames;
}

int32 FGSTN_BalloonMath::WarpEndFrame(double M0, double EndFraction, int32 FlightFrames)
{
	double Consumed = 0.0;
	int32 Frames = 0;
	double M = Multiplier(M0, EndFraction, FlightFrames, Consumed);
	while (M > 1.0 && Frames < FlightFrames)
	{
		Consumed += M;
		++Frames;
		M = Multiplier(M0, EndFraction, FlightFrames, Consumed);
	}
	return Frames;
}
