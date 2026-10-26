// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "GSTN_StatsMovementModifier.generated.h"

class UAbilitySystemComponent;
class UMoverComponent;

/**
 * Permanent Mover modifier that makes the character's speeds follow its stats (combat-m3 section 5). Queued once by
 * UGSTN_CombatSimComponent, never ends.
 *
 * Mover's modes read their speeds from objects that are not part of any sync state (the shared settings, the mode instances), so
 * nothing written there is restored by a rollback. The modifier therefore never keeps or adjusts a value: on every simulated
 * substep, before the modes generate their moves, it recomputes the factor from the ability system (which ticked before Mover in
 * the same frame, so it holds the stats of the frame being simulated, also in a re-simulation) and ASSIGNS archetype value *
 * factor to every speed it owns. The result is a pure function of the restored stats; a resimulated frame, a correction or a mode
 * change cannot leave a stale value behind. The base is the archetype of the object, which no simulation writes.
 *
 * What it writes, for all modes of the component (not only the active one: a mode change inside a substep generates its move
 * after this ran for the old mode):
 *   - UCommonLegacyMovementSettings::MaxSpeed, the value every stock mode and the air control read;
 *   - MaxSpeedOverride of a mode whose archetype sets it (>= 0);
 *   - the floating point properties named in UGSTN_CombatSettings::StatsSpeedProperties, in any mode that has them: the gait
 *     speed variables of a Blueprint walking mode, which feed MaxSpeedOverride and so win over the shared settings.
 *
 * Factor = WalkSpeed / UGSTN_CombatSettings::StatsReferenceWalkSpeed, or 0 while the owner has Combat.Lock.Movement, so the
 * character brakes with its normal deceleration (gravity and layered moves are unaffected). A character whose ability system has
 * no combat attribute set is left untouched.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_StatsMovementModifier : public FMovementModifierBase
{
	GENERATED_BODY()

	FGSTN_StatsMovementModifier();

	virtual void OnPreMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep) override;
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

	/**
	 * The speed factor the stats of ASC ask for: 0 under Combat.Lock.Movement, else WalkSpeed over the reference speed. False
	 * (OutFactor untouched) when ASC has no combat attribute set.
	 */
	static bool ResolveSpeedFactor(const UAbilitySystemComponent& ASC, float& OutFactor);

	/** Assigns the speeds of every mode of MoverComp (see the class comment) archetype * Factor. Returns the number of values written */
	static int32 ApplySpeedFactor(UMoverComponent& MoverComp, float Factor);
};

template<>
struct TStructOpsTypeTraits<FGSTN_StatsMovementModifier> : public TStructOpsTypeTraitsBase2<FGSTN_StatsMovementModifier>
{
	enum
	{
		WithCopy = true
	};
};
