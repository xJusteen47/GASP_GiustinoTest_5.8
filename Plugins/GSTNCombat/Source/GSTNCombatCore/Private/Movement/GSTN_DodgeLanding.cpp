// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_DodgeLanding.h"

#include "Input/GSTN_CombatCommandTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodge_EndLag, "Combat.State.Dodge.EndLag", "In the ending lag of a dodge landing (FGSTN_DodgeLandingState). Projected by the combat tick, never granted");
}

FGSTN_DodgeLandingKitExtension::FGSTN_DodgeLandingKitExtension()
{
	// Applied choices (rule 6, to confirm in playtest): a short belly slide along the landing velocity, whatever the facing
	LandingSlide.bEnabled = true;
	LandingSlide.MinSpeed = 150.f;
	LandingSlide.OppositeDotThreshold = -1.f;
	LandingSlide.Along.Mode = EGSTN_MomentumSlideMode::Slide;
	LandingSlide.Along.Friction = EGSTN_MomentumSlideFriction::Fixed;
	// Owner 2026-11-28: "the dive does not keep its momentum sliding" (4000 braked a 991 cm/s landing in 16 frames, 131 cm). 1500: ~42 frames, ~330 cm
	LandingSlide.Along.Deceleration = 1500.f;
	LandingSlide.Against = LandingSlide.Along;
	LandingSlide.bStopAtLedge = true;
	LandingSlide.bAddRootMotion = false;
	LandingSlide.bUseCarriedSpeed = false;
}

FGSTN_DodgeLandingState FGSTN_DodgeLandingState::Open(int32 InLandedFrame, int32 InEndLagFrames, int32 InLockFrames)
{
	FGSTN_DodgeLandingState State;
	const int32 EndLag = FMath::Clamp(InEndLagFrames, 0, 255);
	const int32 Lock = FMath::Clamp(InLockFrames, 0, 255);
	if (InLandedFrame < 0 || (EndLag == 0 && Lock == 0))
	{
		return State;
	}
	State.LandedFrame = InLandedFrame;
	State.EndLagFrames = static_cast<uint8>(EndLag);
	State.LockFrames = static_cast<uint8>(Lock);
	return State;
}

void FGSTN_DodgeLandingState::CutEndLag(int32 Frame)
{
	if (IsEndLag(Frame))
	{
		EndLagFrames = static_cast<uint8>(FMath::Clamp(Frame - LandedFrame, 0, 255));
	}
}

void FGSTN_DodgeLandingState::NetSerialize(FArchive& Ar)
{
	uint8 bSet = IsSet() ? 1 : 0;
	Ar.SerializeBits(&bSet, 1);
	if (!bSet)
	{
		if (Ar.IsLoading())
		{
			Reset();
		}
		return;
	}
	uint32 Landed = static_cast<uint32>(FMath::Max(LandedFrame, 0));
	Ar.SerializeIntPacked(Landed);
	Ar << EndLagFrames;
	Ar << LockFrames;
	if (Ar.IsLoading())
	{
		LandedFrame = static_cast<int32>(Landed);
	}
}

int32 FGSTN_DodgeLandingLogic::ResolveEndLag(const FGSTN_DodgeLandingKitExtension& Kit, int32 MoveLandingLagFrames)
{
	const int32 Belly = FMath::Max(Kit.EndLagFrames >= 0 ? Kit.EndLagFrames : MoveLandingLagFrames, 0);
	return FMath::Clamp(Belly + FMath::Max(Kit.GetUpRollFrames, 0), 0, 255);
}

int32 FGSTN_DodgeLandingLogic::ResolveLandingLag(const FGSTN_DodgeLandingKitExtension& Kit, int32 MoveLandingLagFrames, float LandingSpeed, float TickRate)
{
	const int32 FixedBelly = FMath::Max(Kit.EndLagFrames >= 0 ? Kit.EndLagFrames : MoveLandingLagFrames, 0);
	int32 Belly = FixedBelly;
	const FGSTN_MomentumSlideResponse& Along = Kit.LandingSlide.Along;
	if (Kit.BellyMode == EGSTN_DodgeBellyMode::UntilStopped && Kit.LandingSlide.bEnabled && Along.Friction == EGSTN_MomentumSlideFriction::Fixed
		&& Along.Deceleration > KINDA_SMALL_NUMBER && TickRate > KINDA_SMALL_NUMBER)
	{
		// The slide starts at MinSpeed or faster and loses Deceleration * dt each frame down to rest
		const float ToBrake = LandingSpeed >= Kit.LandingSlide.MinSpeed ? LandingSpeed : 0.f;
		const int32 SlideFrames = FMath::CeilToInt32(ToBrake / Along.Deceleration * TickRate);
		Belly = FMath::Clamp(SlideFrames, FixedBelly, FMath::Max(Kit.MaxBellyFrames, FixedBelly));
	}
	return FMath::Clamp(Belly + FMath::Max(Kit.GetUpRollFrames, 0), 0, 255);
}

bool FGSTN_DodgeLandingLogic::IsLockedCommand(const FGSTN_DodgeLandingKitExtension& Kit, const FGameplayTag& Command)
{
	if (!Command.IsValid())
	{
		return false;
	}
	// The command and its parents, so a lock on Command.Defence matches Command.Defence.Dodge
	if (Kit.LockedCommands.IsEmpty())
	{
		return Command.MatchesTag(GSTNCommandTags::Defence_Dodge);
	}
	return Command.MatchesAny(Kit.LockedCommands);
}

EGSTN_DodgeLandingGate FGSTN_DodgeLandingLogic::Gate(const FGSTN_DodgeLandingState& State, const FGSTN_DodgeLandingKitExtension& Kit, int32 Frame, const FGameplayTag& Command)
{
	if (!State.IsSet())
	{
		return EGSTN_DodgeLandingGate::Free;
	}
	const bool bLocked = State.IsLocked(Frame) && IsLockedCommand(Kit, Command);
	if (State.IsEndLag(Frame))
	{
		// The jump roll-out is a flag, not a default tag in RecoveryCommands: native tags are not ready when struct defaults are built
		const bool bListed = Command.IsValid() && ((Kit.bJumpRollsOut && Command.MatchesTag(GSTNCommandTags::Movement_Jump))
			|| (!Kit.RecoveryCommands.IsEmpty() && Command.MatchesAny(Kit.RecoveryCommands)));
		const bool bRecovery = !bLocked && bListed && State.EndLagFrameNumber(Frame) >= FMath::Max(Kit.RecoveryFromFrame, 1);
		return bRecovery ? EGSTN_DodgeLandingGate::Recovery : EGSTN_DodgeLandingGate::Refused;
	}
	return bLocked ? EGSTN_DodgeLandingGate::Refused : EGSTN_DodgeLandingGate::Free;
}

void FGSTN_DodgeLandingLogic::Normalize(FGSTN_DodgeLandingState& State, int32 Frame, bool bInReaction)
{
	if (State.IsSet() && (bInReaction || State.HasExpired(Frame)))
	{
		State.Reset();
	}
}
