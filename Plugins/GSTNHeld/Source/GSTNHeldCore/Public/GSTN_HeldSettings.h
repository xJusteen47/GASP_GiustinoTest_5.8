// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_HeldTypes.h"

#include "GSTN_HeldSettings.generated.h"

/** What a player's KO does with the kit (M11 K1, owner decision 5: a dead player leaves the kit; both readings kept as a setting) */
UENUM(BlueprintType)
enum class EGSTN_KitDeathDrop : uint8
{
	/** Nothing drops */
	Off,
	/** The kit drops where the player fell and the player goes back to RespawnKit (the kit really changes hands) */
	DropAndLose,
	/** A copy drops; the player keeps the kit */
	DropCopy
};

/**
 * The kit swap with the grab button (M11 K1, owner decisions 2026-11-11). Every value is a TEST VALUE for playtesting. The swap itself is
 * GSTNCombatGAS's out-of-band loadout write (UGSTN_KitSwapSubsystem::SwapWithPickup): the new kit reaches the simulation as a new
 * LoadoutRevision, swapped at one frame boundary on every machine, which also ends a move that is running (FGSTN_LoadoutSwapLogic).
 */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_KitSwapTuning
{
	GENERATED_BODY()

	/** Kits on the ground are interact targets and every compiled kit gets the swap binding. False = the TH-1b behaviour (hold near a kit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap")
	bool bEnabled = true;

	/**
	 * How long the grab button must be held before its release swaps (decision 6: longer than the object pickup's press, so a kit is never
	 * swapped by mistake). Rounded to combat frames (62.5 Hz)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap", meta = (ClampMin = "0.05", ClampMax = "3.0", ForceUnits = "s"))
	float HoldSeconds = 0.5f;

	/** Priority of the kit meaning (Interact.Target.Kit) beside the pickup's 90 and the grab's 100: equal to the pickup, so the framing decides */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap", meta = (ClampMin = 0, ClampMax = 255))
	int32 MeaningPriority = 90;

	/** True = the kit the interact probe framed on the release; false = simply the nearest kit in reach */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap")
	bool bTakesFocusedKit = true;

	/** When the framed kit is gone (another player took it on the same frame): true = the nearest other kit in reach, false = nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap")
	bool bFocusMissFallsBackToNearest = true;

	/** What a player's KO does with the kit (decision 5). Only pawns with a human player state; dummies and minions never drop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap|Death")
	EGSTN_KitDeathDrop DeathDrop = EGSTN_KitDeathDrop::DropAndLose;

	/** DropAndLose: the kit the player gets back (no support weapon). Empty = the player keeps the kit, as DropCopy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap|Death", meta = (AllowedTypes = "PrimaryKit"))
	FPrimaryAssetId RespawnKit;

	/** A kit dropped on a KO disappears after this long if nobody takes it (0 = never). Kits placed in the level never expire */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap|Death", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DroppedKitLifetimeSeconds = 30.f;

	/** Prompt over the framed kit (placeholder text, local player only, never on a dedicated server) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit Swap")
	bool bShowPrompt = true;

	/** HoldSeconds in combat frames (62.5 Hz), at least 2 so a tap can never swap */
	int32 GetHoldFrames() const { return FMath::Max(2, FMath::RoundToInt(HoldSeconds * 62.5f)); }
};

/**
 * Project defaults of held objects (Project Settings > Plugins > GSTN Held, saved in the plugin's DefaultGSTNHeld.ini). Everything here is a TEST
 * VALUE of the owner's 2026-11-10 button design, kept as data so playtesting changes it without code.
 */
UCLASS(config = GSTNHeld, defaultconfig, meta = (DisplayName = "GSTN Held"))
class GSTNHELDCORE_API UGSTN_HeldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGSTN_HeldSettings& Get() { return *GetDefault<UGSTN_HeldSettings>(); }

	/** When a holder drops what it holds without wanting to; an object's own tuning can override it (bOverrideBreakRules) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Break")
	FGSTN_HeldBreakRules DefaultBreakRules;

	/** How the strong button throws (tap = stick, held = camera aim with the reticle) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Throw")
	FGSTN_HeldThrowInput ThrowInput;

	/**
	 * The kits that get the held bindings (pickup, drop, throw on the default buttons, UGSTN_HeldSlots) through the plugin's loadout extension
	 * when they are compiled, so no kit Blueprint is edited by hand. A kit that binds a held command itself keeps its own binding for that
	 * command. A list, not every kit: a kit that never sees an object keeps its table exactly as authored
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowedClasses = "/Script/GSTNCombatGAS.GSTN_PrimaryKitDefinition"))
	TArray<FSoftObjectPath> KitsWithDefaultBindings;

	/** Every compiled kit gets them, listed or not */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bAddDefaultBindingsToEveryKit = false;

	/** Whether the kit asset at Kit gets the held bindings (the list above, or every kit) */
	bool WantsDefaultBindings(const FSoftObjectPath& Kit) const
	{
		return bAddDefaultBindingsToEveryKit || (!Kit.IsNull() && KitsWithDefaultBindings.Contains(Kit));
	}

	/** The abilities those bindings start; empty = the plugin's own (UGSTN_GA_HeldPickup, UGSTN_GA_HeldDrop, UGSTN_GA_HeldThrow) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath PickupAbility;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath DropAbility;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath ThrowAbility;

	/**
	 * Priority tier of the held commands, handed to the input settings at startup (as GSTNLink does for Command.Link). Without it the held
	 * commands have tier 0 and every strong attack (tier 200) takes the strong press from the throw
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input", meta = (Categories = "Command"))
	TMap<FGameplayTag, int32> CommandTiers;

	/**
	 * Default priority of the pickup meaning (Interact.Target.Pickup) the runtime module adds to the GSTNInteract settings when the project lists
	 * none there (M10 G2). Below the grab's 100, so a grab target wins a tie (INT.5 order). TEST VALUE
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ClampMin = 0, ClampMax = 255))
	int32 PickupMeaningPriority = 90;

	/**
	 * Pickup target (owner, 2026-11-10: both kept as settings for playtesting). True = the object the interact probe frames for the holder;
	 * false = simply the nearest free object in reach, the probe ignored. TEST VALUE
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Interact")
	bool bPickupTakesFocusedObject = true;

	/**
	 * Contested pickup (owner, 2026-11-10, a setting for playtesting): when the framed object cannot be taken (an earlier request got it, gone,
	 * out of reach), true = the nearest other free object, false = the hand closes on nothing. TEST VALUE
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Interact")
	bool bFocusMissFallsBackToNearest = true;

	/**
	 * The size classes (M10 G6), at most four (FGSTN_HeldLoad packs the index in two bits); an object names one by SizeTag. TEST VALUES in
	 * DefaultGSTNHeld.ini: Light, Medium, Heavy, Huge with the plan's rules (Light all, Medium no shield and no air attacks, Heavy walk, drop
	 * and throw only, Huge as Heavy and only with capacity). The slowdown, the throw strength and which objects are liftable are the owner's call
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (TitleProperty = "SizeTag"))
	TArray<FGSTN_HeldSizeClass> SizeClasses;

	/** The carry capacity of a holder whose MaxCarryWeight attribute is 0 (its default): the load is weight / this. TEST VALUE */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0.0", ForceUnits = "kg"))
	float DefaultCarryCapacityKg = 50.f;

	/**
	 * The throw's wind-up of every object that does not override it (slice 10): frames from the button's release to the object leaving the hand
	 * and the placeholder montage. ReleaseFrames 0 = instant, the throw before slice 10. TEST VALUES; gstn.Held.Windup changes it in PIE
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Throw")
	FGSTN_HeldThrowWindup DefaultThrowWindup;

	/** Thrown enemies (M10 G9): which grabbed enemies are lifted and thrown with the held throw, and the thrown body's hitbox. TEST VALUES */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Enemies")
	FGSTN_EnemyThrowTuning ThrownEnemies;

	/** The kit swap with the grab button and the kit drop on a KO (M11 K1). TEST VALUES */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Kits")
	FGSTN_KitSwapTuning KitSwap;

	/** The class of SizeTag and its index (INDEX_NONE and null when none matches exactly, or beyond the four packable ones) */
	const FGSTN_HeldSizeClass* FindSizeClass(const FGameplayTag& SizeTag, int32* OutIndex = nullptr) const;
	/** The class at a payload's index, or null */
	const FGSTN_HeldSizeClass* GetSizeClass(int32 Index) const
	{
		return Index >= 0 && Index < FMath::Min(SizeClasses.Num(), static_cast<int32>(FGSTN_HeldLoad::MaxSizeClasses)) ? &SizeClasses[Index] : nullptr;
	}

	/** Hands CommandTiers to the input settings under this plugin's contribution source */
	void PushToInputSettings() const;
	static void RemoveFromInputSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
