// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_MoveTimelines.h"

int32 FGSTN_MoveTimelines::InvincibleFramesFor(int32 AirFrames, float Fraction)
{
	if (AirFrames <= 0)
	{
		return 0;
	}
	return FMath::Clamp(FMath::RoundToInt32(AirFrames * Fraction), 1, AirFrames);
}

FGSTN_CombatTimelineData FGSTN_MoveTimelines::Make(const FGSTN_MoveTimelineParams& Params)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = Params.TotalFrames;
	Data.FirstActionableFrame = FMath::Clamp(Params.FirstActionableFrame, 0, Params.TotalFrames);
	// A movement is a commitment: no startup override
	Data.OverrideFrames = 0;

	FGSTN_TimelineTrack_Movement Movement;
	Movement.StartFrame = 0;
	Movement.EndFrame = Params.TotalFrames;
	Movement.MixMode = Params.MixMode;
	Movement.MomentumMode = Params.MomentumMode;
	Movement.bCancelOnLand = Params.bCancelOnLand;
	Movement.LandingLagFrames = Params.LandingLagFrames;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Movement>(Movement));

	if (Params.InvincibleFrames > 0)
	{
		FGSTN_TimelineTrack_HurtboxModifier Hurtbox;
		Hurtbox.StartFrame = 0;
		Hurtbox.EndFrame = FMath::Min(Params.InvincibleFrames, Params.TotalFrames);
		Hurtbox.Part.GroupName = Params.HurtboxGroup;
		Hurtbox.Kind = Params.HurtboxKind;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HurtboxModifier>(Hurtbox));
	}
	return Data;
}

FGSTN_DashMoveData FGSTN_MoveTimelines::DashMove()
{
	FGSTN_DashMoveData Move;
	Move.ForwardSpeed = 1500.f;
	return Move;
}

FGSTN_DashMoveData FGSTN_MoveTimelines::GroundDodgeMove()
{
	// A low long-jump arc: about 0.61 s in the air, 46 cm high, 490 cm long under the nominal gravity
	FGSTN_DashMoveData Move;
	Move.ForwardSpeed = 800.f;
	Move.UpSpeed = 300.f;
	Move.ModeChange = EGSTN_DashModeChange::AirMode;
	return Move;
}

FGSTN_DashMoveData FGSTN_MoveTimelines::AirDodgeMove()
{
	// The dive roll: forward and down, whatever the speed was
	FGSTN_DashMoveData Move;
	Move.ForwardSpeed = 700.f;
	Move.UpSpeed = -250.f;
	Move.ModeChange = EGSTN_DashModeChange::AirMode;
	return Move;
}

FGSTN_MoveTimelineParams FGSTN_MoveTimelines::DashParams()
{
	FGSTN_MoveTimelineParams Params;
	Params.TotalFrames = 12;
	Params.FirstActionableFrame = 12;
	return Params;
}

FGSTN_MoveTimelineParams FGSTN_MoveTimelines::GroundDodgeParams(const FGSTN_DashMoveData& Move, float TickRate)
{
	const FGSTN_ArcPrediction Arc = FGSTN_DashMath::PredictArc(Move, NominalGravity, TickRate);
	FGSTN_MoveTimelineParams Params;
	// Room for a heavier gravity than nominal: the ability ends the timeline on landing anyway
	Params.TotalFrames = Arc.AirFrames + 12;
	Params.FirstActionableFrame = Params.TotalFrames;
	Params.bCancelOnLand = true;
	Params.LandingLagFrames = 0;
	Params.InvincibleFrames = InvincibleFramesFor(Arc.AirFrames, DefaultInvincibleFraction);
	return Params;
}

FGSTN_MoveTimelineParams FGSTN_MoveTimelines::AirDodgeParams()
{
	FGSTN_MoveTimelineParams Params;
	Params.TotalFrames = 90;
	Params.FirstActionableFrame = 90;
	Params.bCancelOnLand = true;
	Params.LandingLagFrames = 6;
	Params.InvincibleFrames = InvincibleFramesFor(30, DefaultInvincibleFraction);
	return Params;
}
