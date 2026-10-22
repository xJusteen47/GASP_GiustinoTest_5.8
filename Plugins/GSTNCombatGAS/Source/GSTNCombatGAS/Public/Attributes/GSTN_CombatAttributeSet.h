// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Attributes/GSTN_AttributeAccessors.h"
#include "GSTN_CombatAttributeSet.generated.h"

/**
 * Movement and jump stats of a combatant. Kit stats set them (instant init effect), the active weapon modifies them
 * (infinite stats effect), and the combat movement modifier reads them every Mover tick. Meters live in
 * UGSTN_MeterAttributeSet, and there is deliberately no health here (per-part health is a later milestone).
 *
 * Replication is Network Prediction's job in the GAS-NPP fork, so nothing here replicates. Values change only through
 * gameplay effects, never by a direct setter from simulated code.
 *
 * Clamps follow the fork's idiom: both PreAttributeChange (current value, aggregator results) and PreAttributeBaseChange
 * (base value) apply them, so the value an ability reads and the value an effect writes can never disagree. Every stat is
 * at least 0, and the gravity scales at least MinGravityScale because a zero scale would remove gravity altogether.
 * Starting values come from UGSTN_CombatGASSettings::AttributeDefaults, not from code.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_CombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** Smallest gravity scale an effect can produce. */
	static constexpr float MinGravityScale = 0.05f;

	// Ground movement

	/** Top ground speed, in cm/s. Written to the Mover MaxSpeed by the stats modifier. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement", meta = (ForceUnits = "cm/s"))
	FGameplayAttributeData WalkSpeed;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, WalkSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement", meta = (ForceUnits = "cm/s^2"))
	FGameplayAttributeData Acceleration;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, Acceleration)

	/** Deceleration when there is no input, in cm/s^2. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement", meta = (ForceUnits = "cm/s^2"))
	FGameplayAttributeData Braking;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, Braking)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
	FGameplayAttributeData GroundFriction;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, GroundFriction)

	/** Fraction of the ground acceleration available in the air. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
	FGameplayAttributeData AirControl;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, AirControl)

	// Weight and carrying

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Weight", meta = (ForceUnits = "kg"))
	FGameplayAttributeData Weight;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, Weight)

	/** Heaviest object the combatant can carry. Read from M7. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Weight", meta = (ForceUnits = "kg"))
	FGameplayAttributeData MaxCarryWeight;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, MaxCarryWeight)

	// Jump (frame counts are stored as floats so an effect can scale them, and are rounded down when read)

	/** Launch speed along the local up direction, in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump", meta = (ForceUnits = "cm/s"))
	FGameplayAttributeData JumpVelocity;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, JumpVelocity)

	/** Gravity scale while rising with the jump held. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData JumpRiseGravityScale;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, JumpRiseGravityScale)

	/** Gravity scale while rising after the jump was released. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData JumpCutGravityScale;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, JumpCutGravityScale)

	/** Gravity scale while descending. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData FallGravityScale;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, FallGravityScale)

	/** Terminal fall speed, in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump", meta = (ForceUnits = "cm/s"))
	FGameplayAttributeData MaxFallSpeed;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, MaxFallSpeed)

	/** Terminal fall speed while fast falling, in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump", meta = (ForceUnits = "cm/s"))
	FGameplayAttributeData FastFallSpeed;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, FastFallSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData FastFallGravityScale;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, FastFallGravityScale)

	/** Frames a jump rises before the release can cut it. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData MinJumpHoldFrames;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, MinJumpHoldFrames)

	/** Frames after which holding the jump no longer reduces gravity. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData MaxJumpHoldFrames;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, MaxJumpHoldFrames)

	/** Jumps available while airborne. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData AirJumpCount;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, AirJumpCount)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump", meta = (ForceUnits = "cm/s"))
	FGameplayAttributeData AirJumpVelocity;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, AirJumpVelocity)

	/** Frames between the jump input and the launch. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Jump")
	FGameplayAttributeData JumpSquatFrames;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_CombatAttributeSet, JumpSquatFrames)

	/** Frame-count readers: the stored float rounded down, which is what the simulation must use. */
	int32 GetMinJumpHoldFrameCount() const { return FMath::FloorToInt32(GetMinJumpHoldFrames()); }
	int32 GetMaxJumpHoldFrameCount() const { return FMath::FloorToInt32(GetMaxJumpHoldFrames()); }
	int32 GetAirJumpCountRounded() const { return FMath::FloorToInt32(GetAirJumpCount()); }
	int32 GetJumpSquatFrameCount() const { return FMath::FloorToInt32(GetJumpSquatFrames()); }

	virtual void PostInitProperties() override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

private:
	void ClampStat(const FGameplayAttribute& Attribute, float& NewValue) const;
};
