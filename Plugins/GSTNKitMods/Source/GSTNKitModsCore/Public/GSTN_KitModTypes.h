// Kit mods, pure data (combat-loadout-mods.md §8). No ability system, no world.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "Rules/GSTN_KitExtension.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_KitModTypes.generated.h"

namespace GSTNKitModsTags
{
	/** Root of the mod identity tags (Mod.<Name>); designer leaves live in ini */
	GSTNKITMODSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mod);

	/** Root of the mod slot types (Mod.Slot.Blade, Mod.Slot.Core...); designer leaves live in ini */
	GSTNKITMODSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mod_Slot);
}

/**
 * How a mod changes the moves its filter selects (spec §8). Every scale is 1 = unchanged. Several modifiers on one move multiply
 * (FGSTN_KitModBaking::Combine), then the kit's FGSTN_KitModLimits clamp the result, so no combination breaks a move.
 */
USTRUCT(BlueprintType)
struct GSTNKITMODSCORE_API FGSTN_MoveModifier
{
	GENERATED_BODY()

	/**
	 * Which moves: matched against the move's ability asset tags plus its command tag (a binding of Command.Special.Side carries that
	 * tag). Empty = every move of the kit and its weapons
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Filter")
	FGameplayTagQuery MoveFilter;

	/** Frames of the move: > 1 slower, < 1 faster. Keys keep their order and a window keeps at least one frame; the montage plays at 1/TimeScale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Time", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float TimeScale = 1.f;

	/** Each hitbox's shape extent, in the shape's own axes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Hitbox")
	FVector HitboxScale = FVector::OneVector;

	/** Along each hitbox's authored ReachAxis (M11 L4: the hitbox moves and stretches, a tip sweetspot follows the tip). A hitbox without axis ignores it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Hitbox", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float ReachScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Hit", meta = (ClampMin = "0.0"))
	float DamageScale = 1.f;

	/** Base, growth and fixed knockback */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Hit", meta = (ClampMin = "0.0"))
	float KnockbackScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Hit", meta = (ClampMin = "0.0"))
	float StaggerForceScale = 1.f;

	/** Victim and attacker hitstop multipliers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Hit", meta = (ClampMin = "0.0"))
	float HitstopScale = 1.f;

	/** Tags the move carries for its whole run (a Tags track from frame 0 to the end) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod|Tags")
	FGameplayTagContainer AddedTags;
};

/** The product of the modifiers that select one move, after the kit's limits */
USTRUCT(BlueprintType)
struct GSTNKITMODSCORE_API FGSTN_CombinedMoveModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") float TimeScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") FVector HitboxScale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") float ReachScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") float DamageScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") float KnockbackScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") float StaggerForceScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") float HitstopScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod") FGameplayTagContainer AddedTags;

	/** True when baking would leave the move as it is */
	bool IsIdentity() const;
};

/** The kit's bounds on what mods may do to its moves (spec §8 "Order and stacking") */
USTRUCT(BlueprintType)
struct GSTNKITMODSCORE_API FGSTN_KitModLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods|Limits", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MinTimeScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods|Limits", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float MaxTimeScale = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods|Limits", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float MaxReachScale = 2.f;
};

/**
 * What a kit offers to mods (kit extension, in the kit asset's Extensions): its slots and limits. A kit without it takes no mod.
 * Each entry of Slots is one slot holding one mod (question M11.13: one mod per slot); list a type twice for two slots of it.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Kit mod slots"))
struct GSTNKITMODSCORE_API FGSTN_KitModSlots : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** The slots in slot order; modifiers apply in this order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods", meta = (Categories = "Mod.Slot"))
	TArray<FGameplayTag> Slots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods")
	FGSTN_KitModLimits Limits;

	/**
	 * Most derived timelines one loadout of this kit may add (the registry cap of the card's G4 risk). One derived timeline per distinct
	 * (base timeline, mods) pair; the moves past the cap keep their base timeline and the loadout gets a warning
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods", meta = (ClampMin = "0", ClampMax = "1024"))
	int32 MaxDerivedTimelines = 64;
};

/** One mod the player put in one slot. Ids only: the loadout travels to the server and into the save game */
USTRUCT(BlueprintType)
struct GSTNKITMODSCORE_API FGSTN_EquippedMod
{
	GENERATED_BODY()

	/** The slot type; the mod goes in the first free kit slot of this type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods", meta = (Categories = "Mod.Slot"))
	FGameplayTag Slot;

	/** The mod definition asset (UGSTN_KitModDefinition) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods", meta = (AllowedClasses = "/Script/GSTNKitModsGAS.GSTN_KitModDefinition"))
	FSoftObjectPath Mod;
};

/**
 * The player's mods for one kit. Kit = the kit's identity tag; empty = any kit (the mods follow the player through a kit swap where
 * they fit). Which of the two a game uses is data, so question M11.4 (what a picked-up kit brings) stays open: a kit-specific entry is
 * the player's saved configuration for that kit, a wildcard entry is "keep my mods"
 */
USTRUCT(BlueprintType)
struct GSTNKITMODSCORE_API FGSTN_KitModConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods", meta = (Categories = "Kit"))
	FGameplayTag Kit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods")
	TArray<FGSTN_EquippedMod> Mods;
};

/**
 * The mods in a player's loadout (spec §11.1, wire id "KitMods"). The compile picks the entry of the kit in use
 * (FGSTN_KitModBaking::SelectConfig). On a whole-kit swap from the ground the entry travels with the kit by default, so the ground
 * kit's mods apply (owner answer M11.4); a mode that keeps the player's setups (FGSTN_KitSwapCarryOver::bModsStayWithPlayer) keeps
 * this entry with the player instead, and their saved setup for the new kit applies
 */
USTRUCT(BlueprintType)
struct GSTNKITMODSCORE_API FGSTN_LoadoutModsExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	static const FName WireId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mods")
	TArray<FGSTN_KitModConfig> Configs;

	virtual bool StaysWithPlayerOnKitSwapUnder(const FGSTN_KitSwapCarryOver& Rules) const override { return Rules.bModsStayWithPlayer; }
};
