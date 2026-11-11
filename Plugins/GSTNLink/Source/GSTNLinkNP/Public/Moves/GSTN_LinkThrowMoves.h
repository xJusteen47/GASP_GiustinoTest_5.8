// Links, Network Prediction half: the common throw set (M11 K2).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_LinkThrowMoves.generated.h"

/**
 * The project's common throw set (M11 K2, owner decision 3: every kit has its own four throws, with a common fallback set). A grabbing kit
 * that names no throw for a direction gets these (UGSTN_LinkSettings::FallbackThrows, filled by the loadout extension, no kit edited by hand).
 * Timeline abilities whose frame data is built in the constructor, like the harness moves: a link outcome key Throw whose hit launches the
 * partner in the throw's direction relative to the thrower's facing. PLACEHOLDER VALUES and no montage: K3 gives the test kits their own
 * throws with GASP montages; a Blueprint child of one of these can add a montage and retune the key without code.
 */
UCLASS(Abstract)
class GSTNLINKNP_API UGSTN_GA_LinkThrow : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkThrow(const FObjectInitializer& ObjectInitializer);

protected:
	/**
	 * Builds the move: TotalFrames long, actionable from FirstActionable, the throw key on ThrowFrame. AngleDeg is the launch's elevation
	 * from the thrower's facing; above 90 it leans back over the thrower (the launch math takes cos and sin, so 140 = 40 degrees up, backwards)
	 */
	void BuildThrow(int32 TotalFrames, int32 FirstActionable, int32 ThrowFrame, float Damage, float BaseKnockback, float AngleDeg);
};

/** Forward: 30 frames, key on 8, damage 8, knockback 1600 at 40 degrees ahead (the F12 test throw) */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkThrowForward : public UGSTN_GA_LinkThrow
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkThrowForward(const FObjectInitializer& ObjectInitializer);
};

/** Back: 36 frames, key on 12, damage 10, knockback 1800 at 140 degrees (40 degrees up, behind the thrower): the strong one */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkThrowBack : public UGSTN_GA_LinkThrow
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkThrowBack(const FObjectInitializer& ObjectInitializer);
};

/** Up: 32 frames, key on 10, damage 7, knockback 1700 at 88 degrees (straight up) */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkThrowUp : public UGSTN_GA_LinkThrow
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkThrowUp(const FObjectInitializer& ObjectInitializer);
};

/** Down: 34 frames, key on 14, damage 6, knockback 900 at 80 degrees (a short pop in front: the combo starter) */
UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkThrowDown : public UGSTN_GA_LinkThrow
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkThrowDown(const FObjectInitializer& ObjectInitializer);
};
