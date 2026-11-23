// Test only: the strike kind placeholders made visible in PIE (weakpoint slice 3, scope B). Not a real move.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_TestStrikeKinds.generated.h"

class UGameplayAbility;
class UGSTN_CombatTimeline;
class UGSTN_PrimaryKitDefinition;

/**
 * TEST MOVE, no design value: shows the recoil placeholders of the strike kinds (UGSTN_CombatSettings::SwingRicochetDefault / StabRicochetDefault,
 * the sample shove montages B_A / B_V) on a hard target. Its frame data is the asset /Game/GASNPPTest/Combat/TL_Test_StrikeKinds, which
 * Tools/GAS/setup_strike_kinds_test.py writes with UGSTN_TestStrikeKindsLibrary::FillStrikeKindsTimeline. No recoil of its own anywhere
 * (group, move), so a ricochet falls through the chain to the kind default. Kit: KIT_Test_StrikeKinds (Command.Attack.Neutral).
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestStrikeKinds : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestStrikeKinds(const FObjectInitializer& ObjectInitializer);
};

/** Editor scripting of the strike kind test content (Tools/GAS/setup_strike_kinds_test.py). Every writer returns whether it changed something */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_TestStrikeKindsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The test timeline, relative to an attacker at the origin facing +X (cm), 40 frames, actionable at 34:
	 *   group 0, Swing (FinishThenRecoil by Auto), frames 8..14:  sphere r 40 at hip height sweeping from the right side (40, 140) to the
	 *                                                              front right (140, 40), sweep deflection on
	 *   group 1, Stab (ImmediateRecoil by Auto),  frames 22..26: sphere r 30 thrusting straight ahead from (60, 0) to (170, 0)
	 * A target straight ahead at ~120 cm meets the swing first (its recoil waits for frame 14, then B_A); a target ahead and to the left
	 * (about (130, -50)) is missed by the swing and met by the stab (recoil at once, B_V). Damage 5, knockback 300 at 30 degrees, hitstop 6/4.
	 */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Strike Kinds")
	static bool FillStrikeKindsTimeline(UGSTN_CombatTimeline* Timeline);

	/** Binds Command to Ability in the kit's own bindings (adds the binding when missing) */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Strike Kinds")
	static bool BindKitCommand(UGSTN_PrimaryKitDefinition* Kit, FGameplayTag Command, TSubclassOf<UGameplayAbility> Ability);

	/** Switches off the kit's own recoil fallback, so the chain reaches the strike kind defaults */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Strike Kinds")
	static bool ClearKitRicochetFallback(UGSTN_PrimaryKitDefinition* Kit);

	/** "kit fallback on/off, weapon fallback on/off (weapon name)" of a kit, for the script's log: a weapon fallback would hide the kind default */
	UFUNCTION(BlueprintCallable, Category = "GASNPP Test|Strike Kinds")
	static FString DescribeRecoilChain(const UGSTN_PrimaryKitDefinition* Kit);
};
