#include "Presentation/GSTN_GreyboxAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GSTN_GA_Dash.h"
#include "Rules/GSTN_AirCrouch.h"
#include "Animation/AnimMontage.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Presentation/GSTN_CombatStateLibrary.h"
#include "Presentation/GSTN_RunPresentationComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_GreyboxAnimInstance)

namespace GSTNGreyboxDive
{
	/** The simulation's fixed tick: the dive's frame settings and the combat view's frames are simulation frames */
	constexpr float SimRate = 62.5f;
}

void FGSTN_GreyboxSpring::Step(float Target, float DeltaSeconds, float FrequencyHz, float Damping)
{
	// Sub-steps keep the spring stable on a long frame (a hitch must not explode the capsule)
	const float Omega = 2.f * PI * FMath::Max(FrequencyHz, 0.01f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 120.f)), 1, 16);
	const float Dt = DeltaSeconds / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		const float Accel = Omega * Omega * (Target - Value) - 2.f * Damping * Omega * Velocity;
		Velocity += Accel * Dt;
		Value += Velocity * Dt;
	}
}

float UGSTN_GreyboxAnimInstance::SquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float VelocityZ, float ForwardAcceleration)
{
	if (!bOnGround)
	{
		return FMath::Min(1.f + Settings.AirStretchPer1000 * FMath::Abs(VelocityZ) / 1000.f, Settings.MaxStretch);
	}
	// Only a speed gain squashes (the body gathers to push); braking reads through the lean
	return FMath::Max(1.f - Settings.AccelSquashPer1000 * FMath::Max(ForwardAcceleration, 0.f) / 1000.f, Settings.MinSquash);
}

float UGSTN_GreyboxAnimInstance::PelvisSquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float PelvisHeight,
	float ReferenceHeight)
{
	if (!bOnGround || ReferenceHeight <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	// Only lowering squashes: a pelvis above the reference (a hop inside a clip) keeps the capsule at rest height
	return FMath::Clamp(PelvisHeight / ReferenceHeight, FMath::Min(Settings.MinPelvisSquash, 1.f), 1.f);
}

float UGSTN_GreyboxAnimInstance::LyingTarget(const FGSTN_GreyboxShapeSettings& Settings, float UpZ)
{
	if (!Settings.bLyingFollowsPelvis)
	{
		return 0.f;
	}
	// By the tilt angle from upright (an upside-down pelvis, mid somersault, is past 90: lying)
	const float TiltDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(UpZ, -1.f, 1.f)));
	const float Start = FMath::Min(Settings.LyingStartDeg, Settings.LyingFullDeg);
	const float Full = FMath::Max(Settings.LyingFullDeg, Start + 1.f);
	return FMath::Clamp((TiltDeg - Start) / (Full - Start), 0.f, 1.f);
}

float UGSTN_GreyboxAnimInstance::RollAngle(const FGSTN_GreyboxShapeSettings& Settings, float StartDeg, float MontageShare, float LatchShare)
{
	// The turn's own time: from the latch to the end of the turn share (a latch past it still gets a short turn, never a jump)
	const float Begin = FMath::Clamp(LatchShare, 0.f, 0.99f);
	const float End01 = FMath::Max(Settings.RollTurnShare, Begin + 0.05f);
	const float T = FMath::Clamp((MontageShare - Begin) / (End01 - Begin), 0.f, 1.f);
	// Always forward, toward the travel: from the start round to the next full turn (on the back = 450, head first = 270)
	const float End = 360.f;
	return FMath::Lerp(StartDeg, End, FMath::SmoothStep(0.f, 1.f, T));
}

float UGSTN_GreyboxAnimInstance::DiveFloorRest(float AngleDeg)
{
	return FMath::Abs(FMath::Sin(FMath::DegreesToRadians(AngleDeg)));
}

float UGSTN_GreyboxAnimInstance::DiveAngle(const FGSTN_GreyboxShapeSettings& Settings, float AirSeconds, float LagSeconds, float LagTotalSeconds,
	float LandAngleDeg)
{
	using namespace GSTNGreyboxDive;
	if (LagSeconds < 0.f)
	{
		const float Tilt = FMath::Max(Settings.DiveTiltFrames, 1.f) / SimRate;
		return Settings.DiveAngleDeg * FMath::SmoothStep(0.f, 1.f, FMath::Clamp(AirSeconds / Tilt, 0.f, 1.f));
	}
	// The get-up roll owns the last frames of the lag (all of it when the lag is shorter); the belly part is what comes before
	const float RollSeconds = FMath::Clamp(Settings.DiveGetUpRollFrames / SimRate, 0.f, FMath::Max(LagTotalSeconds, 0.f));
	const float RollStart = FMath::Max(LagTotalSeconds - RollSeconds, 0.f);
	const float Slap = FMath::Max(Settings.DiveSlapFrames, 1.f) / SimRate;
	const float Belly = FMath::Lerp(LandAngleDeg, Settings.DiveBellyDeg, FMath::SmoothStep(0.f, 1.f, FMath::Clamp(LagSeconds / Slap, 0.f, 1.f)));
	if (RollSeconds <= KINDA_SMALL_NUMBER || LagSeconds < RollStart)
	{
		return Belly;
	}
	// Forward over itself, from where the belly part left it round to upright (a full turn)
	const float T = FMath::Clamp((LagSeconds - RollStart) / RollSeconds, 0.f, 1.f);
	return FMath::Lerp(Belly, 360.f, FMath::SmoothStep(0.f, 1.f, T));
}

float UGSTN_GreyboxAnimInstance::RollStartAngle(const FVector& BodyUp, const FVector& Travel)
{
	const FVector Horizontal = FVector(Travel.X, Travel.Y, 0.f).GetSafeNormal();
	return FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(BodyUp | Horizontal), static_cast<float>(BodyUp.Z)));
}

float UGSTN_GreyboxAnimInstance::LimbScaleTarget(const FGSTN_GreyboxShapeSettings& Settings, float AttackWeight)
{
	const float Weight = FMath::Clamp(AttackWeight, 0.f, 1.f);
	const float Locomotion = Settings.bShowLimbsInLocomotion ? 1.f : 0.f;
	const float Moves = Settings.bShowLimbsInMoves ? 1.f : 0.f;
	return FMath::Lerp(Locomotion, Moves, Weight);
}

float UGSTN_GreyboxAnimInstance::FranticAlpha(const FGSTN_GreyboxShapeSettings& Settings, float TimeSinceStart)
{
	if (TimeSinceStart < 0.f)
	{
		return 0.f;
	}
	return 1.f - FMath::Clamp((TimeSinceStart - Settings.FranticSeconds) / FMath::Max(Settings.FranticFadeSeconds, 0.01f), 0.f, 1.f);
}

float UGSTN_GreyboxAnimInstance::SwayCadenceHz(const FGSTN_GreyboxShapeSettings& Settings, float Speed2D, float Frantic)
{
	const float BySpeed = FMath::Max(Speed2D, 0.f) / FMath::Max(Settings.StrideLength, 1.f);
	return FMath::Lerp(BySpeed, FMath::Max(Settings.FranticCadenceHz, BySpeed), FMath::Clamp(Frantic, 0.f, 1.f));
}

float UGSTN_GreyboxAnimInstance::SwayAmplitudeTarget(const FGSTN_GreyboxShapeSettings& Settings, float Speed2D, float Frantic)
{
	const float BySpeed = FMath::Clamp(FMath::Max(Speed2D, 0.f) / FMath::Max(Settings.FullSwaySpeed, 1.f), 0.f, 1.f);
	return FMath::Lerp(BySpeed, Settings.FranticAmplitude, FMath::Clamp(Frantic, 0.f, 1.f));
}

FVector UGSTN_GreyboxAnimInstance::RunSway(const FGSTN_GreyboxShapeSettings& Settings, float Phase, float Amplitude, float& OutPitch)
{
	// Footfalls at phase 0 and PI: the bob dips and the nod tips forward there (|sin| shapes a bounce, not a sine wobble); the rock and
	// the twist swing once per cycle, toward the stepping side
	const float Footfall = 1.f - FMath::Abs(FMath::Sin(Phase));
	OutPitch = -Settings.NodPitchDeg * Footfall * Amplitude;
	return FVector(-Settings.BobSquash * Footfall * Amplitude, Settings.RockRollDeg * FMath::Cos(Phase) * Amplitude,
		Settings.TwistYawDeg * FMath::Sin(Phase) * Amplitude);
}

bool UGSTN_GreyboxAnimInstance::StartsRunStop(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float Speed2D,
	float ForwardAcceleration, bool bAlreadyKicked)
{
	return Settings.RunStopKickDeg > 0.f && bOnGround && !bAlreadyKicked && Speed2D >= Settings.RunStopMinSpeed
		&& ForwardAcceleration <= -Settings.RunStopDecel;
}

FRotator UGSTN_GreyboxAnimInstance::PlantLean(const FVector& LocalMoveDirection, float KickDeg)
{
	const FVector2D Direction = FVector2D(LocalMoveDirection.X, LocalMoveDirection.Y).GetSafeNormal();
	// The top tips against the motion. Pitch up = top back, so a forward motion (+X) gives +pitch; roll = lean right, so a motion to the
	// right (+Y) gives -roll
	return FRotator(Direction.X * KickDeg, 0.f, -Direction.Y * KickDeg);
}

float UGSTN_GreyboxAnimInstance::StickSlip(float CycleFraction)
{
	const float T = FMath::Frac(CycleFraction);
	constexpr float Catch = 0.8f;  // share of the cycle spent catching
	return (T < Catch ? T / Catch : (1.f - T) / (1.f - Catch)) - 0.5f;
}

bool UGSTN_GreyboxAnimInstance::IsCalmForHit(const FGSTN_GreyboxShapeSettings& Settings, int32 HitstopFramesLeft, bool bInHitstun, bool bInTumble)
{
	return Settings.bCalmInHitReactions && (HitstopFramesLeft > 0 || bInHitstun || bInTumble);
}

FRotator UGSTN_GreyboxAnimInstance::LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration)
{
	// Forward (start / brake) and sideways (curves) separately: a curve leans harder than a start, with its own cap
	const FVector2D Lean(FMath::Clamp(LocalAcceleration.X * Settings.LeanDegPer1000 / 1000.f, -Settings.MaxLeanDeg, Settings.MaxLeanDeg),
		FMath::Clamp(LocalAcceleration.Y * Settings.TurnLeanDegPer1000 / 1000.f, -Settings.MaxTurnLeanDeg, Settings.MaxTurnLeanDeg));
	// Pitch = minus the forward lean (Unreal pitch up = top back), roll = the lean to the right (+Y)
	return FRotator(-Lean.X, 0.f, Lean.Y);
}

void UGSTN_GreyboxAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	const AActor* Owner = GetOwningActor();
	Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	SquashSpring.Value = 1.f;
	SquashSpring.Velocity = 0.f;
	bHasPrevious = false;
	Shape.LimbScale = ShapeSettings.bShowLimbsInLocomotion ? 1.f : 0.f;
}

void UGSTN_GreyboxAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	LastDeltaSeconds = DeltaSeconds;
	const AActor* Owner = GetOwningActor();
	if (!Owner || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (!Mover.IsValid())
	{
		Mover = Owner->FindComponentByClass<UMoverComponent>();
	}
	const UMoverComponent* MoverComponent = Mover.Get();
	// The smoothed velocity (2026-11-24): the simulated one changes in 16 ms steps, so a per-frame difference of it is zero on most rendered
	// frames and a spike on the others; the smoothed state's velocity is interpolated like the drawn location and changes every frame
	const FVector Velocity = MoverComponent ? MoverComponent->GetPresentedVelocity() : Owner->GetVelocity();
	const FGSTN_CombatStateView Combat = UGSTN_CombatStateLibrary::GetCombatState(Owner);
	const bool bCalm = IsCalmForHit(ShapeSettings, Combat.HitstopFramesLeft, Combat.bInHitstun, Combat.bInTumble);
	const bool bOnGround = MoverComponent ? MoverComponent->HasGameplayTag(Mover_IsOnGround, true) : true;

	FVector Acceleration = FVector::ZeroVector;
	FVector Acceleration3D = FVector::ZeroVector;
	if (bHasPrevious)
	{
		Acceleration3D = (Velocity - PreviousVelocity) / DeltaSeconds;
		Acceleration = FVector(Acceleration3D.X, Acceleration3D.Y, 0.f);
	}
	const bool bOnWall = MoverComponent && ShapeSettings.WallModeNames.Contains(MoverComponent->GetMovementModeName());
	PreviousVelocity = Velocity;
	bHasPrevious = true;

	// Touchdown: kick the squash spring by the fall speed, so it squashes and bounces back (not in a hit reaction: a knockdown has its own clip;
	// not in a dive: the body lands on its belly, a squash along the lying capsule would shorten it)
	if (bOnGround && !bWasOnGround && LastAirVelocityZ < 0.f && !bCalm && DiveLastAngle <= 0.f)
	{
		const float Omega = 2.f * PI * ShapeSettings.SpringFrequency;
		SquashSpring.Velocity -= ShapeSettings.LandSquashPer1000 * (-LastAirVelocityZ / 1000.f) * Omega;
	}
	if (!bOnGround)
	{
		LastAirVelocityZ = Velocity.Z;
	}
	bWasOnGround = bOnGround;

	// Skid: hold the old facing while the run presentation skids, then turn on a spring (the capsule's yaw offset from the actor)
	if (!RunPresentation.IsValid())
	{
		RunPresentation = Owner->FindComponentByClass<UGSTN_RunPresentationComponent>();
	}
	const bool bSkidding = ShapeSettings.bSkidHoldsFacing && RunPresentation.IsValid() && RunPresentation->IsSkidding();
	const float ActorYaw = Owner->GetActorRotation().Yaw;
	bool bSkidStarted = false;
	if (bSkidding && !bWasSkidding)
	{
		// The velocity still points along the old run on the skid's first frame (the actor may already face the new way)
		const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
		SkidHeldYaw = Horizontal.SizeSquared() > 100.f ? Horizontal.Rotation().Yaw : ActorYaw + TurnSpring.Value;
		bSkidStarted = true;
	}
	if (bSkidding)
	{
		TurnSpring.Value = FMath::FindDeltaAngleDegrees(ActorYaw, SkidHeldYaw);
		TurnSpring.Velocity = 0.f;
	}
	else
	{
		if (bWasSkidding)
		{
			TimeSinceStart = 0.f;  // the turn ends in a fresh (frantic) start
		}
		TurnSpring.Step(0.f, DeltaSeconds, ShapeSettings.SkidTurnFrequency, ShapeSettings.SkidTurnDamping);
	}
	bWasSkidding = bSkidding;
	// The body's frame: the actor turned by the offset; leans and the stop plant read the acceleration in it
	const FQuat BodyQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(TurnSpring.Value)) * Owner->GetActorQuat();

	// The driver's pelvis: its height (the ground squash) and how upright it is (lying)
	float PelvisHeight = 0.f;
	float PelvisReference = 0.f;
	float PelvisUpZ = 1.f;
	const bool bHasPelvis = GetDriverPelvisHeight(PelvisHeight, PelvisReference, PelvisUpZ);
	LyingAlpha = FMath::FInterpTo(LyingAlpha, bHasPelvis ? LyingTarget(ShapeSettings, PelvisUpZ) : 0.f, DeltaSeconds, ShapeSettings.LyingBlendSpeed);
	// A lying body neither leans around its feet nor sways like a runner: a roll moves on the ground but is not a run
	// A roll drives the whole capsule (UpdateRoll): no lying, no lean or sway under it
	bool bRolling = UpdateRoll(Velocity);
	// The dodge dive drives the capsule the same way when no roll montage does (a dodge plays none)
	if (bRolling)
	{
		DiveLastAngle = 0.f;
		DiveLagSeconds = -1.f;
	}
	else
	{
		bRolling = UpdateDive(Velocity, Combat, bOnGround, bCalm, DeltaSeconds);
	}
	if (bRolling)
	{
		LyingAlpha = 0.f;
	}
	const float Upright = bRolling ? 0.f : 1.f - LyingAlpha;

	const FVector Local = BodyQuat.UnrotateVector(Acceleration);
	// On a wall the body is braced like on the ground (no air stretch from the slide's vertical speed), and leans its top off the wall
	const bool bBraced = bOnGround || bOnWall;
	FRotator LeanGoal = bBraced && !bCalm ? LeanTarget(ShapeSettings, Local) : FRotator::ZeroRotator;
	WallNormal = bOnWall ? FindWallNormal(*Owner, BodyQuat) : FVector::ZeroVector;
	const FVector LocalNormal = BodyQuat.UnrotateVector(WallNormal);
	if (!WallNormal.IsNearlyZero())
	{
		LeanGoal.Pitch -= LocalNormal.X * ShapeSettings.WallLeanDeg;
		LeanGoal.Roll += LocalNormal.Y * ShapeSettings.WallLeanDeg;
	}
	// Wall friction (owner 15:26): a held lean against the slide while it brakes, and a stick-slip judder along the slide
	float JudderPitch = 0.f;
	float JudderRoll = 0.f;
	float JudderSquash = 0.f;
	if (bOnWall)
	{
		const float WallSpeed = Velocity.Size();
		const FVector Direction = Velocity.GetSafeNormal();
		const FVector Horizontal(Direction.X, Direction.Y, 0.f);
		const float Brake = FMath::Max(-(Acceleration3D | Direction), 0.f);
		const float BrakeLean = FMath::Min(Brake / 1000.f * ShapeSettings.WallBrakeLeanPer1000, ShapeSettings.WallBrakeMaxLeanDeg);
		// Sideways part: against the motion like the ground plant; downward part: the top tips further off the wall (feet dig in)
		const FRotator Sideways = PlantLean(BodyQuat.UnrotateVector(Horizontal), BrakeLean * Horizontal.Size());
		LeanGoal.Pitch += Sideways.Pitch - LocalNormal.X * BrakeLean * FMath::Abs(Direction.Z);
		LeanGoal.Roll += Sideways.Roll + LocalNormal.Y * BrakeLean * FMath::Abs(Direction.Z);

		const float SpeedAlpha = FMath::Clamp(WallSpeed / FMath::Max(ShapeSettings.WallJudderSpeed, 1.f), 0.f, 1.f);
		const float BrakeAlpha = FMath::Clamp(Brake / FMath::Max(ShapeSettings.WallBrakeFullDecel, 1.f), 0.f, 1.f);
		const float Hz = FMath::Lerp(ShapeSettings.WallJudderMinHz, ShapeSettings.WallJudderMaxHz, SpeedAlpha);
		const float Next = WallJudderCycle + Hz * DeltaSeconds;
		if (Next >= 1.f)
		{
			// Each slip is a little different: a grip is never regular (presentation only, randomness is fine here)
			WallJudderScale = FMath::FRandRange(0.6f, 1.2f);
		}
		WallJudderCycle = FMath::Frac(Next);
		const float Judder = StickSlip(WallJudderCycle) * SpeedAlpha * (1.f + ShapeSettings.WallJudderBrakeBoost * BrakeAlpha) * WallJudderScale;
		// Along the slide: the top is dragged with the motion while it catches and snaps back when it slips (a plant lean has the
		// opposite sign, so the catch is minus it)
		const FRotator Along = PlantLean(BodyQuat.UnrotateVector(Horizontal), -ShapeSettings.WallJudderDeg * Judder * Horizontal.Size());
		JudderPitch = Along.Pitch - LocalNormal.X * ShapeSettings.WallJudderDeg * Judder * FMath::Abs(Direction.Z);
		JudderRoll = Along.Roll + LocalNormal.Y * ShapeSettings.WallJudderDeg * Judder * FMath::Abs(Direction.Z);
		JudderSquash = -ShapeSettings.WallJudderSquash * FMath::Abs(Judder) * 2.f;
	}
	const float SpringDamping = bCalm ? FMath::Max(ShapeSettings.SpringDamping, ShapeSettings.HitReactionSpringDamping) : ShapeSettings.SpringDamping;
	// Air crouch: the body tucks to a fixed squash instead of stretching with the vertical speed (not in a dive, which owns the body)
	const UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const bool bAirCrouch = !bOnGround && !bCalm && DiveLastAngle <= 0.f && OwnerASC && OwnerASC->HasMatchingGameplayTag(GSTNCombatTags::State_AirCrouch);
	const float SquashGoal = bCalm ? 1.f : (bAirCrouch ? ShapeSettings.AirCrouchSquash : SquashTarget(ShapeSettings, bBraced, Velocity.Z, Local.X));
	SquashSpring.Step(SquashGoal, DeltaSeconds, ShapeSettings.SpringFrequency, SpringDamping);
	// Jump squat: squash fast during the sim's squat frames; on take-off the spring is released from there into the air stretch
	const bool bJumpSquatting = UGSTN_RunPresentationComponent::IsAnimOwnerJumpSquatting(this);
	if (bJumpSquatting)
	{
		SquashSpring.Value = FMath::FInterpTo(SquashSpring.Value, ShapeSettings.JumpSquatSquash, DeltaSeconds, ShapeSettings.JumpSquatBlendSpeed);
		SquashSpring.Velocity = 0.f;
	}
	// On a wall the lean is held by the grip, not bounced: critically damped
	const float LeanDamping = bOnWall ? ShapeSettings.WallSpringDamping : SpringDamping;
	PitchSpring.Step(LeanGoal.Pitch, DeltaSeconds, ShapeSettings.SpringFrequency, LeanDamping);
	RollSpring.Step(LeanGoal.Roll, DeltaSeconds, ShapeSettings.SpringFrequency, LeanDamping);

	// Run stop plant: one kick against the motion (owner 14:35: the leg on the side of the motion goes in front), re-armed once still or
	// speeding up again. Braking along the motion, whatever the facing: a sideways or backward brake plants too
	const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.f);
	const float GroundSpeed = GroundVelocity.Size();
	const FVector GroundDirection = GroundVelocity.GetSafeNormal();
	const float BrakeAlong = Acceleration | GroundDirection;
	if (bCalm)
	{
		bRunStopKicked = true;  // a knockback's slide is not a run stop; re-armed once still or speeding up after the reaction
	}
	else if ((bSkidStarted && !bRunStopKicked && ShapeSettings.RunStopKickDeg > 0.f) || StartsRunStop(ShapeSettings, bOnGround, GroundSpeed, BrakeAlong, bRunStopKicked))
	{
		KickPlant(BodyQuat, GroundDirection, ShapeSettings.RunStopKickDeg);
		bRunStopKicked = true;
	}
	else if (GroundSpeed < 10.f || BrakeAlong > 200.f)
	{
		bRunStopKicked = false;
	}
	// Run sway: cadence from the speed, or the frantic cadence right after leaving rest; amplitude from the speed, eased
	const float Speed2D = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	const bool bMoving = bOnGround && Speed2D > 10.f && !bCalm;
	TimeSinceStart = bMoving ? (TimeSinceStart < 0.f ? 0.f : TimeSinceStart + DeltaSeconds) : -1.f;
	if (bSkidding)
	{
		TimeSinceStart = -1.f;  // no frantic sway while sliding; it starts when the turn does
	}
	const float Frantic = FranticAlpha(ShapeSettings, TimeSinceStart);
	// The frantic start replaces the speed's amplitude instead of raising it (run 3: with a max, the fast ramp reached full speed inside
	// the frantic window, so lowering FranticAmplitude changed nothing and the start still rocked as wide as the run, at twice the cadence)
	const float AmplitudeGoal = bMoving ? SwayAmplitudeTarget(ShapeSettings, Speed2D, Frantic) * Upright : 0.f;
	SwayAmplitude = FMath::FInterpTo(SwayAmplitude, AmplitudeGoal, DeltaSeconds, 10.f);
	SwayPhase = FMath::Fmod(SwayPhase + 2.f * PI * SwayCadenceHz(ShapeSettings, Speed2D, Frantic) * DeltaSeconds, 2.f * PI);
	float SwayPitch = 0.f;
	const FVector Sway = RunSway(ShapeSettings, SwayPhase, SwayAmplitude, SwayPitch);

	Shape.Squash = FMath::Clamp(SquashSpring.Value + Sway.X + JudderSquash, ShapeSettings.MinSquash * 0.8f, ShapeSettings.MaxStretch * 1.1f);
	// The lean is in the component's frame; the mesh is rotated -90 yaw from the actor in GASP pawns, so turn the actor-frame lean into it
	const FQuat ActorToComponent = GetSkelMeshComponent() ? (GetSkelMeshComponent()->GetComponentQuat().Inverse() * BodyQuat) : FQuat::Identity;
	// Rotation axis scaled by degrees, actor frame: tipping the top toward +X is a turn around +Y by the forward lean (-Pitch), toward +Y a
	// turn around -X by the side lean (Roll)
	const float Roll = (RollSpring.Value + Sway.Y + JudderRoll) * Upright;
	const float Pitch = (PitchSpring.Value + SwayPitch + JudderPitch) * Upright;
	const FVector Axis = ActorToComponent.RotateVector(FVector(-Roll, -Pitch, 0.f));
	const FQuat Tip = Axis.IsNearlyZero() ? FQuat::Identity : FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(Axis.Size()));
	// Twist about the up axis (same in actor and component frames), under the tip
	Shape.Lean = (Tip * FQuat(FVector::UpVector, FMath::DegreesToRadians(TurnSpring.Value + Sway.Z))).Rotator();

	// A lying body is not a crouch: its low pelvis lies the capsule down (LyingWeight) instead of squashing it
	const float PelvisGoal = bRolling ? 1.f
		: FMath::Lerp(bHasPelvis ? PelvisSquashTarget(ShapeSettings, bOnGround, PelvisHeight, PelvisReference) : 1.f, 1.f, LyingAlpha);
	// Follows the pose directly (no spring: the clip already eases), blended only where it switches on or off
	Shape.PelvisSquash = FMath::FInterpTo(Shape.PelvisSquash, PelvisGoal, DeltaSeconds, ShapeSettings.PelvisSquashBlendSpeed);

	UpdateMoveOverride();
	// Bones only where strictly needed, in attacks (owner 2026-11-24): the body follows the pose and the markers show only while an
	// attack's capsule clip plays; everything else (locomotion, hit reactions, rolls) is the rigid capsule swaying
	Shape.PoseWeight = Shape.OverrideWeight;
	// A move's capsule clip that lies down (the dropkick's landing) lies on the floor, not at its key pose's height
	Shape.bFloorRestByTilt = Shape.OverrideClip && Shape.OverrideWeight > 0.f;
	Shape.LyingWeight = LyingAlpha;
	// The floor only on the ground, or just under a falling pawn (the frame a move lands, the drawn pose can be the landing's while the ground
	// tag is read a frame late: 2026-11-28, the dropkick's land pose drawn one frame at its key height, 87 cm over the floor). Its height is
	// measured, not taken as the mesh root's: a per-move capsule can hold the root under the floor
	float FloorHeight = 0.f;
	const bool bFloorFound = ShapeSettings.bKeepAboveFloor && FindFloorHeight(*Owner, FloorHeight);
	const bool bFloorLift = bFloorFound && (bOnGround || Velocity.Z <= 0.f);
	Shape.FloorRadius = bFloorLift ? ShapeSettings.CapsuleRadius : 0.f;
	Shape.FloorHeight = bFloorLift ? FloorHeight : 0.f;
	Shape.FloorBottom = ShapeSettings.CapsuleBottom;
	Shape.FloorTop = ShapeSettings.CapsuleTop;
	Shape.LimbScale = FMath::FInterpTo(Shape.LimbScale, LimbScaleTarget(ShapeSettings, Shape.OverrideWeight), DeltaSeconds,
		ShapeSettings.LimbBlendSpeed);
}

void UGSTN_GreyboxAnimInstance::KickPlant(const FQuat& BodyQuat, const FVector& WorldMoveDirection, float KickDeg)
{
	if (KickDeg <= KINDA_SMALL_NUMBER || WorldMoveDirection.IsNearlyZero())
	{
		return;
	}
	const FRotator Kick = PlantLean(BodyQuat.UnrotateVector(WorldMoveDirection), KickDeg);
	// For this damping the spring's first peak is about 0.6 of Velocity / Omega
	const float Gain = 2.f * PI * ShapeSettings.SpringFrequency / 0.6f;
	PitchSpring.Velocity += Kick.Pitch * Gain;
	RollSpring.Velocity += Kick.Roll * Gain;
}

bool UGSTN_GreyboxAnimInstance::FindFloorHeight(const AActor& Owner, float& OutHeight) const
{
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const UWorld* World = Owner.GetWorld();
	if (!Capsule || !World)
	{
		return false;
	}
	// Presentation only: one trace down the movement capsule's axis, a floor at most FloorProbe under its bottom (the capsule stands on it
	// when grounded; falling, it is about to)
	constexpr float FloorProbe = 15.f;
	const FVector Start = Owner.GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, Owner.GetSimpleCollisionHalfHeight() + FloorProbe);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNGreyboxFloor), false, &Owner);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || Hit.ImpactNormal.Z <= 0.7f)
	{
		return false;
	}
	// Component space height (the mesh only turns about the up)
	OutHeight = static_cast<float>(Hit.ImpactPoint.Z - Capsule->GetComponentLocation().Z);
	return true;
}

FVector UGSTN_GreyboxAnimInstance::FindWallNormal(const AActor& Owner, const FQuat& BodyQuat) const
{
	const UWorld* World = Owner.GetWorld();
	if (!World)
	{
		return FVector::ZeroVector;
	}
	// Presentation only: four short traces around the body, nearest wall-like hit wins (the wall mode's own normal lives in the game
	// module's sync state, which this plugin must not read)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNGreyboxWall), false, &Owner);
	const FVector Start = Owner.GetActorLocation();
	const float Reach = Owner.GetSimpleCollisionRadius() + ShapeSettings.WallProbeDistance;
	FVector Best = FVector::ZeroVector;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FVector& Axis : {FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector})
	{
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, Start + BodyQuat.RotateVector(Axis) * Reach, ECC_Visibility, Params)
			&& FMath::Abs(Hit.ImpactNormal.Z) < 0.7f && Hit.Distance < BestDistance)
		{
			BestDistance = Hit.Distance;
			Best = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
		}
	}
	return Best;
}

bool UGSTN_GreyboxAnimInstance::UpdateRoll(const FVector& Velocity)
{
	Shape.Spin = FQuat::Identity;
	Shape.FloorRest = 0.f;
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const USkeletalMeshComponent* Driver = Capsule ? Cast<USkeletalMeshComponent>(Capsule->GetAttachParent()) : nullptr;
	UAnimInstance* DriverAnim = Driver ? Driver->GetAnimInstance() : nullptr;
	const UAnimMontage* Montage = DriverAnim ? DriverAnim->GetCurrentActiveMontage() : nullptr;
	const FAnimMontageInstance* Instance = Montage ? DriverAnim->GetActiveInstanceForMontage(Montage) : nullptr;
	bool bRollMontage = false;
	if (ShapeSettings.bProceduralRoll && Instance && Montage->GetPlayLength() > KINDA_SMALL_NUMBER)
	{
		const FString Name = Montage->GetName();
		for (const FString& Pattern : ShapeSettings.RollMontageNames)
		{
			bRollMontage |= !Pattern.IsEmpty() && Name.Contains(Pattern);
		}
	}
	if (!bRollMontage)
	{
		RollMontage.Reset();
		bRollLatched = false;
		return false;
	}
	if (RollMontage.Get() != Montage)
	{
		RollMontage = Montage;
		bRollLatched = false;
	}
	// The direction is the travel once the roll moves (a roll back travels against the facing); until then the driver's pose stays
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	if (!bRollLatched)
	{
		if (Horizontal.Size() < ShapeSettings.RollMinSpeed || !Capsule)
		{
			return false;
		}
		RollTravel = Horizontal.GetSafeNormal();
		// How the capsule lay as the roll got going: the driver pelvis's up (the capsule followed it), in world space
		const FVector WorldUp = Driver->GetComponentQuat().RotateVector(PelvisUp);
		RollStartDeg = FMath::Lerp(0.f, RollStartAngle(WorldUp, RollTravel), LyingAlpha);
		RollLatchShare = Instance->GetPosition() / Montage->GetPlayLength();
		bRollLatched = true;
	}
	const float Share = Instance->GetPosition() / Montage->GetPlayLength();
	const float Angle = RollAngle(ShapeSettings, RollStartDeg, Share, RollLatchShare);
	// Turning the top toward the travel is a turn about Up x Travel; in the capsule component's frame, about the capsule's middle
	const FVector WorldAxis = FVector::CrossProduct(FVector::UpVector, RollTravel).GetSafeNormal();
	const FVector Axis = Capsule->GetComponentQuat().UnrotateVector(WorldAxis);
	if (Axis.IsNearlyZero())
	{
		return false;
	}
	Shape.Spin = FQuat(Axis, FMath::DegreesToRadians(Angle));
	Shape.SpinPivot = FVector(0.f, 0.f, 0.5f * (ShapeSettings.CapsuleBottom + ShapeSettings.CapsuleTop));
	return true;
}

bool UGSTN_GreyboxAnimInstance::UpdateDive(const FVector& Velocity, const FGSTN_CombatStateView& Combat, bool bOnGround, bool bCalm, float DeltaSeconds)
{
	using namespace GSTNGreyboxDive;
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const AActor* Owner = GetOwningActor();
	auto Stop = [this]()
	{
		DiveLastAngle = 0.f;
		DiveLagSeconds = -1.f;
		DiveMoveStartFrame = INDEX_NONE;
		return false;
	};
	// A hit ends the dive at once: the reaction has its own read
	if (!ShapeSettings.bProceduralDive || bCalm || !Capsule || !Owner)
	{
		return Stop();
	}
	// A dodge is the ability that opens a dodge landing (the ground and the air dodge, and any kit child of them)
	const UGSTN_GA_MoveTimeline* MoveCDO = Combat.bInMove && Combat.MoveAbility ? Cast<UGSTN_GA_MoveTimeline>(Combat.MoveAbility->GetDefaultObject()) : nullptr;
	const bool bDodgeInAir = MoveCDO && MoveCDO->bDodgeLanding && !bOnGround;
	const bool bInLag = Combat.DodgeEndLagFrame > 0 && Combat.DodgeEndLagFrames > 0;

	// The travel the body tips toward: the horizontal velocity, eased (air control can curve it), the facing before it moves
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	if (DiveLastAngle <= 0.f || DiveTravel.IsNearlyZero())
	{
		DiveTravel = Horizontal.Size() >= ShapeSettings.RollMinSpeed ? Horizontal.GetSafeNormal() : FVector(Owner->GetActorForwardVector().X, Owner->GetActorForwardVector().Y, 0.f).GetSafeNormal();
	}
	else if (Horizontal.Size() >= ShapeSettings.RollMinSpeed)
	{
		DiveTravel = (DiveTravel + (Horizontal.GetSafeNormal() - DiveTravel) * FMath::Min(12.f * DeltaSeconds, 1.f)).GetSafeNormal();
	}

	float Angle = 0.f;
	if (bInLag)
	{
		if (DiveLagSeconds < 0.f)
		{
			DiveLagSeconds = 0.f;
			DiveLandAngle = DiveLastAngle > 0.f ? DiveLastAngle : ShapeSettings.DiveAngleDeg;
		}
		else
		{
			DiveLagSeconds += DeltaSeconds;
		}
		// Rendered time, kept within the simulation frame the view is on (smooth between frames, never drifting from the sim)
		const float SimSeconds = (Combat.DodgeEndLagFrame - 1) / SimRate;
		DiveLagSeconds = FMath::Clamp(DiveLagSeconds, SimSeconds, SimSeconds + 1.f / SimRate);
		Angle = DiveAngle(ShapeSettings, 0.f, DiveLagSeconds, Combat.DodgeEndLagFrames / SimRate, DiveLandAngle);
	}
	else if (bDodgeInAir)
	{
		if (Combat.MoveStartFrame != DiveMoveStartFrame)
		{
			DiveMoveStartFrame = Combat.MoveStartFrame;
			DiveAirSeconds = 0.f;
		}
		DiveAirSeconds += DeltaSeconds;
		DiveLagSeconds = -1.f;
		Angle = DiveAngle(ShapeSettings, DiveAirSeconds, -1.f, 0.f, 0.f);
	}
	else if (DiveLastAngle > 0.f && DiveLastAngle < 360.f)
	{
		// Cut short (a roll-out command, the move ended before the lag, a landing not seen yet): finish the turn quickly, forward when it was
		// already lying or past it, back to upright when it had barely tipped
		const float Step = ShapeSettings.DiveRecoverDegPerSecond * DeltaSeconds;
		Angle = DiveLastAngle < 45.f ? FMath::Max(DiveLastAngle - Step, 0.f) : FMath::Min(DiveLastAngle + Step, 360.f);
		if (Angle <= 0.f || Angle >= 360.f)
		{
			return Stop();
		}
	}
	else
	{
		return Stop();
	}
	DiveLastAngle = Angle;

	// Same turn as the roll: the top toward the travel, about Up x Travel, around the capsule's middle (the floor lift keeps it out of the floor)
	const FVector WorldAxis = FVector::CrossProduct(FVector::UpVector, DiveTravel).GetSafeNormal();
	const FVector Axis = Capsule->GetComponentQuat().UnrotateVector(WorldAxis);
	if (Axis.IsNearlyZero())
	{
		return false;
	}
	Shape.Spin = FQuat(Axis, FMath::DegreesToRadians(Angle));
	Shape.SpinPivot = FVector(0.f, 0.f, 0.5f * (ShapeSettings.CapsuleBottom + ShapeSettings.CapsuleTop));
	// On the ground the turned capsule lies on the floor (owner 2026-11-28: "the capsule stays lying in mid-air"): turned about its middle it
	// hangs half its length minus its radius over the feet. As much as it lies: none upright, all of it on the belly
	Shape.FloorRest = bOnGround ? DiveFloorRest(Angle) : 0.f;
	return true;
}

bool UGSTN_GreyboxAnimInstance::GetDriverPelvisHeight(float& OutHeight, float& OutReference, float& OutUpZ)
{
	OutUpZ = 1.f;
	static const FName PelvisName(TEXT("pelvis"));
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const USkeletalMeshComponent* Driver = Capsule ? Cast<USkeletalMeshComponent>(Capsule->GetAttachParent()) : nullptr;
	const USkeletalMesh* DriverMesh = Driver ? Driver->GetSkeletalMeshAsset() : nullptr;
	if (!DriverMesh)
	{
		return false;
	}
	if (PelvisReferenceMesh.Get() != DriverMesh)
	{
		// A skin swap changes the driver mesh: measure its reference pelvis again (component space = above the feet in GASP pawns)
		const FReferenceSkeleton& RefSkeleton = DriverMesh->GetRefSkeleton();
		const int32 Index = RefSkeleton.FindBoneIndex(PelvisName);
		PelvisReferenceMesh = DriverMesh;
		const FTransform RefPelvis = Index != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index) : FTransform::Identity;
		PelvisReferenceHeight = Index != INDEX_NONE ? static_cast<float>(RefPelvis.GetLocation().Z) : 0.f;
		PelvisLocalUp = RefPelvis.GetRotation().UnrotateVector(FVector::UpVector);
	}
	if (PelvisReferenceHeight <= KINDA_SMALL_NUMBER || Driver->GetBoneIndex(PelvisName) == INDEX_NONE)
	{
		return false;
	}
	const FTransform Pelvis = Driver->GetSocketTransform(PelvisName, RTS_Component);
	OutHeight = static_cast<float>(Pelvis.GetLocation().Z);
	OutReference = PelvisReferenceHeight;
	PelvisUp = Pelvis.GetRotation().RotateVector(PelvisLocalUp);
	OutUpZ = static_cast<float>(PelvisUp.Z);
	return true;
}

void UGSTN_GreyboxAnimInstance::UpdateMoveOverride()
{
	Shape.OverridePelvisFollow = ShapeSettings.bMoveClipFollowsDriverPelvis ? 1.f : 0.f;
	FindMoveOverride();
	// When the montage ends the clip fades out from its last pose instead of leaving in one frame (2026-11-28, the dropkick's get-up: the
	// capsule clip stood a few frames before the driver, and dropped back onto the rising driver for one frame when the montage ended)
	const float FadeSeconds = ShapeSettings.MoveClipFadeOutFrames / 60.f;
	if (Shape.OverrideClip && Shape.OverrideWeight > 0.f)
	{
		LastOverrideClip = Shape.OverrideClip;
		LastOverrideTime = Shape.OverrideTime;
		LastOverrideWeight = Shape.OverrideWeight;
	}
	else if (LastOverrideClip.IsValid() && LastOverrideWeight > 0.f && FadeSeconds > 0.f)
	{
		LastOverrideWeight = FMath::Max(LastOverrideWeight - LastDeltaSeconds / FadeSeconds, 0.f);
		Shape.OverrideClip = LastOverrideWeight > 0.f ? LastOverrideClip.Get() : nullptr;
		Shape.OverrideTime = LastOverrideTime;
		Shape.OverrideWeight = LastOverrideWeight;
	}
	else
	{
		LastOverrideClip = nullptr;
		LastOverrideWeight = 0.f;
	}
}

void UGSTN_GreyboxAnimInstance::FindMoveOverride()
{
	Shape.OverrideClip = nullptr;
	Shape.OverrideWeight = 0.f;
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const USkeletalMeshComponent* Driver = Capsule ? Cast<USkeletalMeshComponent>(Capsule->GetAttachParent()) : nullptr;
	UAnimInstance* DriverAnim = Driver ? Driver->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = DriverAnim ? DriverAnim->GetCurrentActiveMontage() : nullptr;
	const FAnimMontageInstance* Instance = Montage ? DriverAnim->GetActiveInstanceForMontage(Montage) : nullptr;
	if (!Instance)
	{
		return;
	}
	if (MoveOverrides.IsEmpty() && SequenceMoveOverrides.IsEmpty())
	{
		return;
	}
	const float Position = Instance->GetPosition();

	// The segment under the montage's time: a move built from several sequences (the dropkick: startup, loop repeated in the air until
	// it lands, land) needs a capsule clip per sequence, because the loop's length in the montage is not the time it is really shown
	const FAnimTrack* Track = Montage->SlotAnimTracks.IsEmpty() ? nullptr : &Montage->SlotAnimTracks[0].AnimTrack;
	const int32 SegmentCount = Track ? Track->AnimSegments.Num() : 0;
	const FAnimSegment* Segment = nullptr;
	if (SegmentCount > 0)
	{
		const int32 Index = Track->GetSegmentIndexAtTime(Position);
		Segment = &Track->AnimSegments[Index != INDEX_NONE ? Index : (Position <= 0.f ? 0 : SegmentCount - 1)];
	}

	// 1) By montage asset for a one-sequence montage (resolved once per montage; a synchronous load of a small clip, presentation only)
	if (Montage->IsAsset() && SegmentCount <= 1 && ApplyMontageOverride(*Montage, *Instance, Position))
	{
		return;
	}
	// 2) By the sequence the montage plays at its time (dynamic montages, multi-sequence montages, a montage without its own entry)
	if (Segment && ApplySequenceOverride(*Montage, *Instance, *Segment, Position))
	{
		return;
	}
	// 3) A multi-sequence montage without per-sequence clips falls back to its whole-montage clip
	if (Montage->IsAsset() && SegmentCount > 1)
	{
		ApplyMontageOverride(*Montage, *Instance, Position);
	}
}

bool UGSTN_GreyboxAnimInstance::ApplyMontageOverride(UAnimMontage& Montage, const FAnimMontageInstance& Instance, float Position)
{
	TObjectPtr<UAnimSequenceBase>* Loaded = LoadedOverrides.Find(&Montage);
	if (!Loaded)
	{
		const TSoftObjectPtr<UAnimSequenceBase>* Soft = MoveOverrides.Find(TSoftObjectPtr<UAnimMontage>(&Montage));
		Loaded = &LoadedOverrides.Add(&Montage, Soft ? Soft->LoadSynchronous() : nullptr);
	}
	if (!*Loaded)
	{
		return false;
	}
	Shape.OverrideClip = *Loaded;
	Shape.OverrideTime = Position;
	Shape.OverrideWeight = FMath::Clamp(Instance.GetWeight(), 0.f, 1.f);
	return true;
}

bool UGSTN_GreyboxAnimInstance::ApplySequenceOverride(const UAnimMontage& Montage, const FAnimMontageInstance& Instance, const FAnimSegment& Segment,
	float Position)
{
	UAnimSequenceBase* Played = Segment.GetAnimReference();
	if (!Played)
	{
		return false;
	}
	TObjectPtr<UAnimSequenceBase>* Loaded = LoadedSequenceOverrides.Find(Played);
	if (!Loaded)
	{
		const TSoftObjectPtr<UAnimSequenceBase>* Soft = SequenceMoveOverrides.Find(TSoftObjectPtr<UAnimSequenceBase>(Played));
		Loaded = &LoadedSequenceOverrides.Add(Played, Soft ? Soft->LoadSynchronous() : nullptr);
		if (!*Loaded)
		{
			// Once per clip: names what to add to SequenceMoveOverrides (make_capsule_clips.py reads this line from the log)
			UE_LOG(LogAnimation, Log, TEXT("GSTN Greybox: no capsule override for montage %s playing %s"), *Montage.GetName(), *Played->GetPathName());
		}
	}
	if (!*Loaded)
	{
		return false;
	}
	Shape.OverrideClip = *Loaded;
	// The time inside the played sequence (a looping segment wraps), mapped onto the capsule clip's length
	const float SequenceTime = Segment.ConvertTrackPosToAnimPos(Position);
	const float SequenceLength = Played->GetPlayLength();
	Shape.OverrideTime = SequenceLength > KINDA_SMALL_NUMBER ? SequenceTime / SequenceLength * (*Loaded)->GetPlayLength() : SequenceTime;
	Shape.OverrideWeight = FMath::Clamp(Instance.GetWeight(), 0.f, 1.f);
	return true;
}
