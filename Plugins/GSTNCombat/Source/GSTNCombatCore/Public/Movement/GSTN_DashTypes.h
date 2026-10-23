// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_DashTypes.generated.h"

/** Which movement mode the character is in after a dash or dodge impulse */
UENUM(BlueprintType)
enum class EGSTN_DashModeChange : uint8
{
	/** The impulse keeps the current mode: a ground dash stays on the ground (walking friction then slows it) */
	KeepMode,

	/** The impulse leaves the ground for the air mode of the movement settings: the arc of a ground dodge, the dive of an air dodge */
	AirMode
};

/**
 * The speeds of one dash or dodge impulse, as data of a kit's gameplay effect. Directions are relative to the local gravity frame:
 * Up is -GravityDir, "forward" is the facing projected onto the plane perpendicular to Up. Nothing here names world Z.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_DashMoveData
{
	GENERATED_BODY()

	/** Speed along the facing, in the plane perpendicular to the local up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float ForwardSpeed = 0.f;

	/** Speed along the local up. Positive throws the character into an arc (long jump), negative dives */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash", meta = (ForceUnits = "cm/s"))
	float UpSpeed = 0.f;

	/** Additive adds the impulse to the velocity the character kept (see MomentumMode); Override replaces it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	EGSTN_MoveMixMode MixMode = EGSTN_MoveMixMode::Override;

	/** What happens to the velocity the character had at the start (the same meaning as on the movement track) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	EGSTN_MomentumMode MomentumMode = EGSTN_MomentumMode::Discard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	EGSTN_DashModeChange ModeChange = EGSTN_DashModeChange::KeepMode;
};

/** What a ballistic arc does on level ground under constant gravity (a prediction for tuning and tests, never used to move anything) */
struct FGSTN_ArcPrediction
{
	/** Seconds from take-off to the return to the take-off height. 0 when the impulse does not leave the ground upwards */
	float AirTimeSeconds = 0.f;

	/** Highest point above the take-off height */
	float ApexHeight = 0.f;

	/** Distance covered along the forward direction while airborne */
	float Range = 0.f;

	/** AirTimeSeconds in frames of the fixed tick, rounded up */
	int32 AirFrames = 0;
};

/** Pure math of the dash and dodge impulses in the local gravity frame */
struct GSTNCOMBATCORE_API FGSTN_DashMath
{
	/**
	 * The unit direction of Facing projected onto the plane perpendicular to Up. When the facing is (nearly) parallel to Up the
	 * Fallback is projected instead; zero when that fails too.
	 */
	static FVector HorizontalForward(const FVector& Up, const FVector& Facing, const FVector& Fallback);

	/** The velocity the impulse adds: Forward * ForwardSpeed + Up * UpSpeed. Up and Forward must be unit vectors */
	static FVector ImpulseVelocity(const FVector& Up, const FVector& Forward, const FGSTN_DashMoveData& Data);

	/**
	 * The velocity after the impulse given the one before it: Discard drops it; Preserve and ClearAtEnd keep it, and the mix mode
	 * decides between adding to it (Additive) and replacing it (Override)
	 */
	static FVector ResolveVelocity(const FVector& Prior, const FVector& Impulse, EGSTN_MoveMixMode Mix, EGSTN_MomentumMode Momentum);

	/** The ballistic arc of the impulse under a gravity of size GravityAccel (cm/s^2) and a fixed tick of TickRate Hz */
	static FGSTN_ArcPrediction PredictArc(const FGSTN_DashMoveData& Data, float GravityAccel, float TickRate);
};
