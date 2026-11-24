// Things in the world that a spin or a drill dive turns: screws, knobs, valves, fans, switches (spin-drill.md 13c).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "GSTN_SpinTurnable.generated.h"

/** What turned a turnable */
UENUM(BlueprintType)
enum class EGSTN_SpinTurnSource : uint8
{
	/** A spin started near it (ground, air or chained; one call per spin) */
	Spin,
	/** A drill dive ended on it (the dive's ground lag; one call per dive) */
	Drill,
};

UINTERFACE(BlueprintType, Blueprintable, meta = (DisplayName = "GSTN Spin Turnable"))
class GSTNSPINDRILLCORE_API UGSTN_SpinTurnable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement it (in C++ or on any Blueprint actor, Class Settings > Interfaces) and the spin turns the actor: the world's
 * UGSTN_SpinTurnSubsystem calls OnSpinTurned once for every spin that starts within reach of it and every drill dive that lands on it.
 *
 * Why an interface and not a combat target: level objects are not simulated by Network Prediction. The call comes only on the authority, from
 * the authoritative events Combat.Event.Spin.Started / Combat.Event.Drill.GroundLag (published once, never re-simulated), so the object keeps
 * ordinary replicated state and nothing on it is ever rolled back. The price: no prediction (a client sees the turn after the round trip) and
 * the drill does not latch onto it (a latch needs a combat target; a turnable that must be bored in continuously is a part, combat-parts.md 3.4).
 *
 * Sense convention (level-activation.md 3): +1 clockwise, -1 counter-clockwise, as seen from above along the spinner's up.
 */
class GSTNSPINDRILLCORE_API IGSTN_SpinTurnable
{
	GENERATED_BODY()

public:
	/**
	 * Called on the authority only. Turner = the spinning pawn. Sense = +1 clockwise, -1 counter-clockwise. Source = spin or drill dive:
	 * how many steps each is worth is the object's own data (spin-drill.md SD.13, designer's choice per object)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SpinDrill|Turnable")
	void OnSpinTurned(AActor* Turner, int32 Sense, EGSTN_SpinTurnSource Source);

	/** False = ignore turns for now (already unscrewed, disabled by a level script...). Default true */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SpinDrill|Turnable")
	bool CanBeSpinTurned(AActor* Turner) const;
};
