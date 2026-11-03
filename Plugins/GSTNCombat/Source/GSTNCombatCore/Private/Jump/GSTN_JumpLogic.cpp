// Pure combat logic (no GAS, no Network Prediction).

#include "Jump/GSTN_JumpLogic.h"

double FGSTN_JumpLogic::UpSpeed(const FVector& Velocity, const FVector& GravityDir)
{
	return -Velocity.Dot(GravityDir.GetSafeNormal());
}

FGSTN_JumpState FGSTN_JumpLogic::Step(const FGSTN_JumpState& In, int32 Frame, const FGSTN_JumpMoverSample& Sample, bool bButtonHeld, const FGSTN_JumpParams& Params, const FGSTN_JumpResetRules& Rules)
{
	FGSTN_JumpState Out = In;

	if (Sample.bGrounded)
	{
		if (In.SquatFramesLeft > 0)
		{
			// The squat runs on the ground: the take-off is the frame it runs out, as a request of that frame would have been
			if (In.SquatFramesLeft == 1)
			{
				FGSTN_JumpState Takeoff = ApplyTakeoff(In, Frame, In.Button, EGSTN_JumpKind::Ground);
				Takeoff.SquatFramesLeft = 0;
				return Takeoff;
			}
			Out.SquatFramesLeft = In.SquatFramesLeft - 1;
			return Out;
		}
		Out = FGSTN_JumpState();
		Out.CoyoteFramesLeft = static_cast<uint8>(FMath::Clamp(Params.CoyoteFrames, 0, 255));
		return Out;
	}

	// Off the ground (knocked, pushed) the squat is over without a jump
	Out.SquatFramesLeft = 0;

	if (Out.CoyoteFramesLeft > 0)
	{
		--Out.CoyoteFramesLeft;
	}

	const bool bRising = Sample.UpSpeed > 0.0;

	if (Sample.bExternalLaunch)
	{
		switch (Rules.AirJumpsOnExternalLaunch)
		{
		case EGSTN_AirJumpsOnLaunch::Refill:
			Out.AirJumpsUsed = 0;
			break;
		case EGSTN_AirJumpsOnLaunch::SetCount:
			Out.AirJumpsUsed = static_cast<uint8>(FMath::Clamp(Params.AirJumpCount - Rules.AirJumpsSetCount, 0, 255));
			break;
		case EGSTN_AirJumpsOnLaunch::Keep:
			break;
		}
		if (Rules.CoyoteOnExternalLaunch == EGSTN_CoyoteOnLaunch::Clear)
		{
			Out.CoyoteFramesLeft = 0;
		}

		// A launch ends a fast fall. A jump in its hold phase keeps shaping the rise unless the rules end it
		Out.bFastFalling = false;
		if (!bRising)
		{
			Out.Phase = EGSTN_JumpPhase::Falling;
		}
		else if (Rules.bCancelHoldPhaseOnExternalLaunch || Out.Phase != EGSTN_JumpPhase::Rising)
		{
			Out.Phase = EGSTN_JumpPhase::Launched;
			Out.StartFrame = INDEX_NONE;
			Out.Button = INDEX_NONE;
			Out.bJumpCut = false;
		}
		return Out;
	}

	switch (Out.Phase)
	{
	case EGSTN_JumpPhase::None:
		// Airborne with nothing of ours going on: walked off a ledge, or knocked off the ground
		Out.Phase = bRising ? EGSTN_JumpPhase::Launched : EGSTN_JumpPhase::Falling;
		break;

	case EGSTN_JumpPhase::Rising:
		if (!Out.bJumpCut)
		{
			const int32 Elapsed = Frame - Out.StartFrame;
			if (Elapsed >= Params.MaxHoldFrames || (!bButtonHeld && Elapsed >= Params.MinHoldFrames))
			{
				Out.bJumpCut = true;
			}
		}
		if (!bRising)
		{
			Out.Phase = EGSTN_JumpPhase::Falling;
		}
		break;

	case EGSTN_JumpPhase::Launched:
		if (!bRising)
		{
			Out.Phase = EGSTN_JumpPhase::Falling;
		}
		break;

	case EGSTN_JumpPhase::Falling:
		break;
	}
	return Out;
}

EGSTN_JumpKind FGSTN_JumpLogic::ClassifyJump(const FGSTN_JumpState& State, bool bGrounded, const FGSTN_JumpParams& Params)
{
	if (State.SquatFramesLeft > 0)
	{
		return EGSTN_JumpKind::None;
	}
	if (bGrounded || State.CoyoteFramesLeft > 0)
	{
		return EGSTN_JumpKind::Ground;
	}
	return State.AirJumpsUsed < Params.AirJumpCount ? EGSTN_JumpKind::Air : EGSTN_JumpKind::None;
}

bool FGSTN_JumpLogic::CanFastFall(const FGSTN_JumpState& State, bool bGrounded)
{
	return !bGrounded && State.Phase == EGSTN_JumpPhase::Falling && !State.bFastFalling;
}

FGSTN_JumpState FGSTN_JumpLogic::ApplyTakeoff(const FGSTN_JumpState& In, int32 Frame, int32 Button, EGSTN_JumpKind Kind)
{
	FGSTN_JumpState Out = In;
	Out.Phase = EGSTN_JumpPhase::Rising;
	Out.StartFrame = Frame;
	Out.Button = static_cast<int8>(Button);
	Out.bJumpCut = false;
	Out.bFastFalling = false;
	Out.CoyoteFramesLeft = 0;
	Out.bTakeoffWasAir = Kind == EGSTN_JumpKind::Air;
	if (Kind == EGSTN_JumpKind::Air && Out.AirJumpsUsed < 255)
	{
		++Out.AirJumpsUsed;
	}
	return Out;
}

FGSTN_JumpState FGSTN_JumpLogic::ApplyRequest(const FGSTN_JumpState& In, int32 Frame, int32 Button, EGSTN_JumpKind Kind, bool bGrounded, const FGSTN_JumpParams& Params)
{
	if (Kind == EGSTN_JumpKind::Ground && bGrounded && Params.SquatFrames > 0)
	{
		FGSTN_JumpState Out = In;
		Out.SquatFramesLeft = static_cast<uint8>(FMath::Clamp(Params.SquatFrames, 1, 255));
		Out.Button = static_cast<int8>(Button);
		return Out;
	}
	return ApplyTakeoff(In, Frame, Button, Kind);
}

FGSTN_JumpState FGSTN_JumpLogic::ApplyFastFall(const FGSTN_JumpState& In)
{
	FGSTN_JumpState Out = In;
	Out.bFastFalling = true;
	return Out;
}

float FGSTN_JumpLogic::GravityScale(const FGSTN_JumpState& State, const FGSTN_JumpParams& Params)
{
	if (State.bFastFalling)
	{
		return Params.FastFallGravityScale;
	}
	switch (State.Phase)
	{
	case EGSTN_JumpPhase::Rising:
		return State.bJumpCut ? Params.CutGravityScale : Params.RiseGravityScale;
	case EGSTN_JumpPhase::Falling:
		return Params.FallGravityScale;
	case EGSTN_JumpPhase::None:
	case EGSTN_JumpPhase::Launched:
	default:
		return 1.f;
	}
}

float FGSTN_JumpLogic::LimitedGravityScale(const FGSTN_JumpState& State, const FGSTN_JumpParams& Params, double DownSpeed, double GravityMagnitude, double DeltaSeconds)
{
	const float Scale = GravityScale(State, Params);
	const float Cap = State.bFastFalling ? Params.FastFallSpeed : Params.MaxFallSpeed;
	const double Step = GravityMagnitude * Scale * DeltaSeconds;
	if (Cap <= 0.f || Step <= 0.0)
	{
		return Scale;
	}
	const double Headroom = Cap - DownSpeed;
	if (Headroom <= 0.0)
	{
		return 0.f;
	}
	return Headroom >= Step ? Scale : static_cast<float>(Scale * Headroom / Step);
}

FGSTN_JumpApex FGSTN_JumpLogic::SimulateApex(const FVector& GravityDir, double GravityMagnitude, double JumpSpeed, int32 HoldFrames, const FGSTN_JumpParams& Params, double DeltaTime)
{
	const FVector Down = GravityDir.GetSafeNormal();
	const FVector Up = -Down;
	const FGSTN_JumpResetRules NoRules;

	FGSTN_JumpState State = ApplyTakeoff(FGSTN_JumpState(), 0, 0, EGSTN_JumpKind::Ground);
	FVector Velocity = Up * JumpSpeed;
	FVector Position = FVector::ZeroVector;
	FGSTN_JumpApex Apex;

	// Long enough for any sane stat set; the loop ends once the character descends
	for (int32 Frame = 1; Frame < 10000; ++Frame)
	{
		FGSTN_JumpMoverSample Sample;
		Sample.bGrounded = false;
		Sample.UpSpeed = UpSpeed(Velocity, Down);
		State = Step(State, Frame, Sample, Frame <= HoldFrames, Params, NoRules);

		Velocity += Down * (GravityMagnitude * GravityScale(State, Params) * DeltaTime);
		Position += Velocity * DeltaTime;

		const double Height = Position.Dot(Up);
		if (Height > Apex.Height)
		{
			Apex.Height = Height;
			Apex.Position = Position;
			Apex.Frames = Frame;
		}
		else if (State.Phase == EGSTN_JumpPhase::Falling)
		{
			break;
		}
	}
	return Apex;
}
