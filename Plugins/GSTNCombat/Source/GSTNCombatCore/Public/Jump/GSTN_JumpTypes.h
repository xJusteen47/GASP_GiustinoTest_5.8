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

	/** Frames after leaving the ground in which a ground jump is still allowed (0 = none) */
	int32 CoyoteFrames = 4;
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

	bool IsDefault() const
	{
		return Phase == EGSTN_JumpPhase::None && StartFrame == INDEX_NONE && Button == INDEX_NONE && !bJumpCut && !bFastFalling && !bTakeoffWasAir
			&& AirJumpsUsed == 0 && CoyoteFramesLeft == 0;
	}

	bool operator==(const FGSTN_JumpState& Other) const
	{
		return Phase == Other.Phase && StartFrame == Other.StartFrame && Button == Other.Button && bJumpCut == Other.bJumpCut && bFastFalling == Other.bFastFalling
			&& bTakeoffWasAir == Other.bTakeoffWasAir && AirJumpsUsed == Other.AirJumpsUsed && CoyoteFramesLeft == Other.CoyoteFramesLeft;
	}

	/** One bit when default; same code for both directions */
	void NetSerialize(FArchive& Ar);

	void ToString(FAnsiStringBuilderBase& Out) const;
};
