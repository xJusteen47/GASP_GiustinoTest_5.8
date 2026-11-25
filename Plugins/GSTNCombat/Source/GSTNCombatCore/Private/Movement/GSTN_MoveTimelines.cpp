// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_MoveTimelines.h"

#include "Hits/GSTN_CombatHitTags.h"

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
	Movement.AirControlOverride = Params.AirControlOverride;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Movement>(Movement));

	if (Params.HurtboxFrames > 0)
	{
		FGSTN_TimelineTrack_HurtboxModifier Hurtbox;
		Hurtbox.StartFrame = 0;
		Hurtbox.EndFrame = FMath::Min(Params.HurtboxFrames, Params.TotalFrames);
		// By tag: a hurtbox has no group of its own, and the tag is what the set's designer classifies the feet with
		if (Params.HurtboxTag.IsValid())
		{
			Hurtbox.AffectedTags.AddTag(Params.HurtboxTag);
		}
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
	// A low long-jump arc: about 0.61 s in the air, 46 cm high, 490 cm long under the nominal gravity. Mario long jump (owner 2026-11-17): a run
	// keeps its speed into the jump (never slower than 800), up to 1400
	FGSTN_DashMoveData Move;
	Move.ForwardSpeed = 800.f;
	Move.UpSpeed = 300.f;
	Move.ModeChange = EGSTN_DashModeChange::AirMode;
	Move.CarriedSpeedScale = 1.f;
	Move.MaxForwardSpeed = 1400.f;
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
	// Owner 2026-11-17 20:13Z: the dodge needs ending lag ("vulnerable at the end" meant lag). 8 frames from the landing, counted from 1, as the
	// move's data; a kit overrides it with FGSTN_DodgeLandingKitExtension::EndLagFrames. The combat state holds the lag (FGSTN_DodgeLandingState),
	// so it lasts even if a late landing outruns the timeline
	Params.LandingLagFrames = DefaultDodgeEndLagFrames;
	// Room for a heavier gravity than nominal and for the lag: the ability ends the timeline after the lag anyway
	Params.TotalFrames = Arc.AirFrames + 12 + Params.LandingLagFrames + DefaultDodgeGetUpRollFrames;
	Params.FirstActionableFrame = Params.TotalFrames;
	Params.bCancelOnLand = true;
	Params.HurtboxFrames = InvincibleFramesFor(Arc.AirFrames, DefaultInvincibleFraction);
	Params.HurtboxTag = GSTNCombatHitTags::Hurtbox_Feet.GetTag();
	// Owner 2026-11-17: no startup (the impulse is the first frame), blocking until the end of the lag, the feet vulnerable after the first 40 %,
	// and highly controllable in the air (Mario long jump: steer to choose the landing)
	Params.AirControlOverride = 1.f;
	return Params;
}

FGSTN_MoveTimelineParams FGSTN_MoveTimelines::AirDodgeFromGroundParams(const FGSTN_DashMoveData& Move, float TickRate)
{
	FGSTN_MoveTimelineParams Params = GroundDodgeParams(Move, TickRate);
	Params.TotalFrames = FMath::Max(Params.TotalFrames, AirDodgeMaxAirFrames + Params.LandingLagFrames + DefaultDodgeGetUpRollFrames);
	Params.FirstActionableFrame = Params.TotalFrames;
	return Params;
}

FGSTN_MoveTimelineParams FGSTN_MoveTimelines::AirDodgeParams()
{
	FGSTN_MoveTimelineParams Params;
	Params.TotalFrames = 90;
	Params.FirstActionableFrame = 90;
	Params.bCancelOnLand = true;
	Params.LandingLagFrames = 6;
	Params.HurtboxFrames = InvincibleFramesFor(30, DefaultInvincibleFraction);
	Params.HurtboxTag = GSTNCombatHitTags::Hurtbox_Feet.GetTag();
	return Params;
}
