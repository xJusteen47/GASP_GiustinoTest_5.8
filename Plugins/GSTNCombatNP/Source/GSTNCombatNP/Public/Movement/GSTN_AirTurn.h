// Combat simulation (GAS-NPP fork): the air turn at the kit profile's rate (polish M3, qa-movement-1117 defect 3).

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "GSTN_AirTurn.generated.h"

struct FMoverTickStartData;
struct FProposedMove;

/**
 * Input entry the combat component adds to the Mover command on the frames its air turn writes the orientation intent
 * (UGSTN_CombatSimComponent::ProduceInput_Implementation, FGSTN_AnalogWalkTuning::AirTurnRateDegPerSec). It travels with the command,
 * so the owner's prediction, its replays and the server all see the same rate: nothing about the turn lives on a component.
 *
 * Why it exists: GASP's BP_MovementMode_Falling overrides GenerateMove, calls Mover's falling move and then recomputes the angular
 * velocity with ComputeAngularVelocityDegrees and a FIXED TurningRateLimit of 300 deg/s typed on the node. Mover's shared TurningRate
 * (raised to the air turn rate in the air by FGSTN_RunStageMover::ApplyAirValues) never reaches that node, so the profile's 3600 was
 * capped to 300 (180 degrees in ~38 frames). FGSTN_AirTurn::Apply sets the turn again after the mode, from this entry.
 * The Blueprint is GASP sample content and stays untouched.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_AirTurnInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	/** Turn rate for this frame, deg/s (the profile's air turn rate). 0 = no air turn this frame. Sent as whole degrees per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Air Turn", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float RateDegPerSec = 0.f;

	virtual FMoverDataStructBase* Clone() const override { return new FGSTN_AirTurnInputs(*this); }
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_AirTurnInputs> : public TStructOpsTypeTraitsBase2<FGSTN_AirTurnInputs>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

/** The air turn inside Mover's tick, pure apart from its arguments (same start state, same command, same output on every machine) */
struct GSTNCOMBATNP_API FGSTN_AirTurn
{
	/** Largest rate the entry can carry (uint16 on the wire) */
	static constexpr float MaxRateDegPerSec = 65535.f;

	/** Rounds a rate the way NetSerialize sends it, so the producer predicts with the value the server will read */
	static float QuantizeRate(float RateDegPerSec);

	/**
	 * Replaces the yaw part of the generated move's angular velocity with the air turn: from the start orientation toward the command's
	 * orientation intent at the entry's rate, about Up. Only when: the command carries a positive FGSTN_AirTurnInputs and an orientation
	 * intent, bFallingMode (the frame's mode is a UFallingMode, GASP's Blueprint one included), and no layered move overrides the
	 * velocity (a knockback, a slide, a freeze own the rotation then). Pitch and roll are dropped: the falling mode keeps the body upright.
	 * Returns true when the move was changed.
	 */
	static bool Apply(const FMoverTickStartData& Start, bool bFallingMode, const FVector& Up, float DeltaSeconds, FProposedMove& InOutMove);
};
