// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Misc/StringBuilder.h"
#include "GSTN_JumpTypes.generated.h"

/** The part of a jump the combatant is in, which picks the gravity scale (FGSTN_JumpLogic::GravityScale) */
UENUM(BlueprintType)
enum class EGSTN_JumpPhase : uint8
{
	/** Grounded, or airborne for the one frame before the state notices how it got there */
	None,

	/** Rising after a jump of the combatant's own: the hold gravity while the button is held, the cut gravity after */
	Rising,

	/** Descending, whether from a jump or from walking off a ledge */
	Falling,

	/** Rising because something else launched the combatant (a trampoline): plain gravity, no hold phase */
	Launched
};

/**
 * How the gravity of a jump of the combatant's own is chosen while it rises (owner 2026-11-17: both behaviours stay as options, per kit).
 * Both: instant impulse at the take-off, JumpRiseGravityScale while the button is held, JumpCutGravityScale once it is released (after
 * MinJumpHoldFrames), FallGravityScale once descending. They differ only on what MaxJumpHoldFrames ends.
 */
UENUM(BlueprintType)
enum class EGSTN_JumpGravityModel : uint8
{
	/**
	 * Owner's model, a facsimile of the early 2D Mario jumps (default): MaxJumpHoldFrames is a window from the take-off. Inside it the held
	 * button keeps the reduced (rise) gravity and a release switches to the boosted (cut) gravity; when the window ends the descent (fall)
	 * gravity applies, still rising or not, held or not
	 */
	HoldWindow,

	/**
	 * The behaviour before 2026-11-18 (Mario 3D World preset): reaching MaxJumpHoldFrames counts as a release, the cut gravity applies until
	 * the apex, the fall gravity after it
	 */
	CutUntilApex
};

/**
 * Running jump (owner 2026-11-18, option per kit, OFF by default): a ground jump taken faster goes higher, as in the early 2D Mario games (SMB1
 * gives its fastest jump 5 px/f instead of 4: about +25 % height). The take-off speed JumpVelocity is scaled by VelocityScale of the horizontal
 * speed at the take-off (the speed across local up, from the Mover sync state of the frame before: the same on every machine). Linear between
 * MinSpeed (scale 1) and FullSpeed (MaxVelocityScale); the height grows with the square of the scale (1.12 = +25 %). Air jumps are never boosted.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_JumpRunBoost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump")
	bool bEnabled = false;

	/** Horizontal speed at and below which the jump is the plain one, cm/s. Default: run stage 1 (470) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump", meta = (EditCondition = "bEnabled", ClampMin = "0", ForceUnits = "cm/s"))
	float MinSpeed = 470.f;

	/** Horizontal speed at and above which the full boost applies, cm/s. Default: run stage 3, sprint 2 (850) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump", meta = (EditCondition = "bEnabled", ClampMin = "0", ForceUnits = "cm/s"))
	float FullSpeed = 850.f;

	/** Take-off speed multiplier at FullSpeed. 1.12 = about +25 % height (SMB1's fastest jump; also Mario 3D World JumpPow / JumpPowLow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "2"))
	float MaxVelocityScale = 1.12f;

	/** The multiplier of JumpVelocity for a ground jump taken at HorizontalSpeed (1 when disabled) */
	float VelocityScale(double HorizontalSpeed) const
	{
		if (!bEnabled || MaxVelocityScale <= 1.f)
		{
			return 1.f;
		}
		if (FullSpeed <= MinSpeed)
		{
			return HorizontalSpeed >= MinSpeed ? MaxVelocityScale : 1.f;
		}
		const float Alpha = FMath::Clamp(static_cast<float>((HorizontalSpeed - MinSpeed) / (FullSpeed - MinSpeed)), 0.f, 1.f);
		return FMath::Lerp(1.f, MaxVelocityScale, Alpha);
	}
};

/** What an external launch does to the air jumps already used */
UENUM(BlueprintType)
enum class EGSTN_AirJumpsOnLaunch : uint8
{
	/** Leave the count alone */
	Keep,

	/** Every air jump is available again */
	Refill,

	/** Exactly AirJumpsSetCount air jumps are available (capped by the air jump stat) */
	SetCount
};

/** What an external launch does to the coyote window */
UENUM(BlueprintType)
enum class EGSTN_CoyoteOnLaunch : uint8
{
	/** The launch ends the window: the ground is no longer under the feet, the jump left is an air jump */
	Clear,

	/** The window runs on */
	Keep
};

/**
 * How the jump state reacts when something other than the combatant's own jump launches it (a trampoline, a launcher: a
 * FLayeredMove_Launch that the Mover state tracker reports as WasExternallyLaunchedThisFrame). Data, not code: a kit can make a
 * trampoline refill the air jumps, or keep them, without touching the framework. Deterministic: the same on every machine.
 * The default refills the air jumps, clears the coyote window and ends the hold phase of a jump in progress.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_JumpResetRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump")
	EGSTN_AirJumpsOnLaunch AirJumpsOnExternalLaunch = EGSTN_AirJumpsOnLaunch::Refill;

	/** For SetCount: the air jumps available after the launch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump", meta = (ClampMin = "0", EditCondition = "AirJumpsOnExternalLaunch == EGSTN_AirJumpsOnLaunch::SetCount"))
	int32 AirJumpsSetCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump")
	EGSTN_CoyoteOnLaunch CoyoteOnExternalLaunch = EGSTN_CoyoteOnLaunch::Clear;

	/** End the variable-jump gravity phase of a jump in progress (the launch sets the rising speed, so the hold must not keep shaping it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Jump")
	bool bCancelHoldPhaseOnExternalLaunch = true;
};

/**
 * The stats and settings the jump state machine reads, as plain numbers so the logic stays pure: the combat attribute set
 * fills them in the simulation. Frame counts are whole frames.
 */
struct GSTNCOMBATCORE_API FGSTN_JumpParams
{
	float RiseGravityScale = 1.f;
	float CutGravityScale = 2.f;
	float FallGravityScale = 1.25f;
	float FastFallGravityScale = 2.f;
	int32 MinHoldFrames = 4;
	int32 MaxHoldFrames = 12;
	int32 AirJumpCount = 1;

	/** Terminal speeds along local down in cm/s (<= 0 = no limit): MaxFallSpeed for a plain fall, FastFallSpeed while fast falling. See FGSTN_JumpLogic::LimitedGravityScale */
	float MaxFallSpeed = 0.f;
	float FastFallSpeed = 0.f;

	/** Frames after leaving the ground in which a ground jump is still allowed (0 = none) */
	int32 CoyoteFrames = 4;

	/** Frames between a ground jump request and its take-off (0 = the take-off is the frame of the request) */
	int32 SquatFrames = 0;

	/** Frames over which the gravity grows from RiseGravityScale to CutGravityScale after the jump cut (0 = at once) */
	int32 CutRampFrames = 0;

	/** What MaxHoldFrames ends (EGSTN_JumpGravityModel). Kit data (UGSTN_GA_PlatformJump::GravityModel) */
	EGSTN_JumpGravityModel GravityModel = EGSTN_JumpGravityModel::HoldWindow;

	/**
	 * Glide (M11 L7b, the jet-pack's slow descent): while falling with the button of the last jump still held, gravity uses GlideGravityScale
	 * and the terminal speed GlideMaxFallSpeed. <= 0 = no glide (every kit without a glide stat, the default). Fast fall wins over the glide
	 */
	float GlideGravityScale = 0.f;

	/**
	 * Terminal speed along local down while gliding, cm/s (<= 0 = the plain MaxFallSpeed). Like every terminal speed here it caps the
	 * acceleration only: a fall already faster when the glide starts keeps its speed. A brake would need a negative gravity, which flips
	 * Mover's up (it follows the gravity), so it is not done here
	 */
	float GlideMaxFallSpeed = 0.f;

	bool CanGlide() const { return GlideGravityScale > 0.f; }
};

/**
 * Jump state of the combat sync state: simulated, rolled back and sent like the rest of it. Integers and flags only.
 * Written by the combat tick (FGSTN_JumpLogic) and nowhere else; the jump ability and the stats modifier read it. A combatant
 * that never jumped holds the defaults and costs one bit on the wire.
 */
struct GSTNCOMBATCORE_API FGSTN_JumpState
{
	EGSTN_JumpPhase Phase = EGSTN_JumpPhase::None;

	/** Frame the rising began, INDEX_NONE when no jump of the combatant's own is in progress */
	int32 StartFrame = INDEX_NONE;

	/** Index of the button that started the jump (its hold shapes the height), INDEX_NONE when none */
	int8 Button = INDEX_NONE;

	/** The hold phase is over: the cut gravity applies while still rising */
	bool bJumpCut = false;

	bool bFastFalling = false;

	/** The take-off of StartFrame was an air jump (the ability reads it to pick the launch speed) */
	bool bTakeoffWasAir = false;

	uint8 AirJumpsUsed = 0;

	/** Frames left in which a ground jump is allowed after leaving the ground */
	uint8 CoyoteFramesLeft = 0;

	/** Frames of the ground squat still to go before the take-off (0 = not squatting). Phase stays None and Button holds the jump button meanwhile */
	uint8 SquatFramesLeft = 0;

	/** Falling with the button of the last jump held and a glide stat set (FGSTN_JumpParams::CanGlide): the glide gravity applies. Written by Step */
	bool bGliding = false;

	/** Frames since the jump cut while still rising (saturates at 255): the cut gravity ramp (FGSTN_JumpParams::CutRampFrames) */
	uint8 CutFrames = 0;

	/** HoldWindow model only: MaxHoldFrames passed while rising, the fall gravity applies until the landing. Written by Step */
	bool bHoldWindowOver = false;

	bool IsDefault() const
	{
		return Phase == EGSTN_JumpPhase::None && StartFrame == INDEX_NONE && Button == INDEX_NONE && !bJumpCut && !bFastFalling && !bTakeoffWasAir
			&& AirJumpsUsed == 0 && CoyoteFramesLeft == 0 && SquatFramesLeft == 0 && !bGliding && CutFrames == 0 && !bHoldWindowOver;
	}

	bool operator==(const FGSTN_JumpState& Other) const
	{
		return Phase == Other.Phase && StartFrame == Other.StartFrame && Button == Other.Button && bJumpCut == Other.bJumpCut && bFastFalling == Other.bFastFalling
			&& bTakeoffWasAir == Other.bTakeoffWasAir && AirJumpsUsed == Other.AirJumpsUsed && CoyoteFramesLeft == Other.CoyoteFramesLeft && SquatFramesLeft == Other.SquatFramesLeft
			&& bGliding == Other.bGliding && CutFrames == Other.CutFrames && bHoldWindowOver == Other.bHoldWindowOver;
	}

	/** One bit when default; same code for both directions */
	void NetSerialize(FArchive& Ar);

	void ToString(FAnsiStringBuilderBase& Out) const;
};
