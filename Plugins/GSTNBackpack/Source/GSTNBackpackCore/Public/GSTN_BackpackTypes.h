// Backpacks, pure data (combat-loadout-mods.md §10). No ability system, no world.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_BackpackTypes.generated.h"

namespace GSTNBackpackTags
{
	/** Root of the backpack identity tags (Backpack.<Name>); designer leaves live in ini */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack);

	/**
	 * The logical command of a backpack's main verb (spec §10). Only the command is native: which button enters it is data (the
	 * backpack binding's slot, or the kit's command set), never fixed here (question M11.11)
	 */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Backpack_Use);

	/** The command of the shield-dome backpack (M11 L7b): its slot is Press(dodge) with the stick neutral, so it beats the plain dodge only then */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Backpack_Dome);

	/** Identities of the two starter backpacks (UGSTN_BackpackDefinition_JetPack, _ShieldDome); other backpacks name theirs in ini */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack_JetPack);
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack_ShieldDome);

	/** Projected from the backpack fragment each frame (never added by hand): memory slot N has no charge left. A binding gates on it (IgnoreTags) */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Empty_Slot0);
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Empty_Slot1);
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Empty_Slot2);
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Empty_Slot3);

	/** Projected: the entity holds a dome up */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dome);

	/** The empty tag of memory slot Slot (invalid outside 0..3) */
	GSTNBACKPACKCORE_API FGameplayTag EmptySlotTag(int32 Slot);
}

/**
 * The jet-pack's movement (owner 2026-11-12: a double jump, and a slow descent while the jump button is held). Added to the carrier's combat
 * stats while the backpack is carried: ExtraAirJumps adds to AirJumpCount (the kit's own jump command and air jump state do the rest), the
 * glide values set the jump state's glide (FGSTN_JumpParams). Defaults are a first guess for the playtest: the owner tunes them. All 0 = no
 * movement change (every backpack but the jet-pack).
 */
USTRUCT(BlueprintType)
struct GSTNBACKPACKCORE_API FGSTN_BackpackMovement
{
	GENERATED_BODY()

	/** Air jumps added to the kit's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack|Movement", meta = (ClampMin = "0", ClampMax = "5"))
	int32 ExtraAirJumps = 0;

	/** Gravity scale while falling with the jump held; 0 = no glide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlideGravityScale = 0.f;

	/** Terminal fall speed while gliding; 0 = the kit's own */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float GlideMaxFallSpeed = 0.f;

	bool IsNone() const { return ExtraAirJumps == 0 && GlideGravityScale <= 0.f; }
};

/**
 * The backpack in a player's loadout (spec §11.1, wire id "Backpack"). One backpack at a time; ids only, because the loadout travels to
 * the server and into the save game. Kept through a whole-kit swap by default (spec §10: "a kit change keeps the backpack"); a game that
 * wants the backpack to travel with the kit sets bKeepOnKitSwap false in the loadout it hands out.
 */
USTRUCT(BlueprintType)
struct GSTNBACKPACKCORE_API FGSTN_LoadoutBackpackExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	static const FName WireId;

	/** The backpack asset (UGSTN_BackpackDefinition). Empty = no backpack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack", meta = (AllowedClasses = "/Script/GSTNBackpackGAS.GSTN_BackpackDefinition"))
	FSoftObjectPath Backpack;

	/** Stays with the player when they take a whole kit from the ground (true), or goes down with the kit they leave (false) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack")
	bool bKeepOnKitSwap = true;

	virtual bool StaysWithPlayerOnKitSwap() const override { return bKeepOnKitSwap && Backpack.IsValid(); }
};

/** The pure backpack rules on a loadout; the same answer on every machine */
struct GSTNBACKPACKCORE_API FGSTN_BackpackLoadoutRules
{
	/** The backpack the loadout carries, empty when none */
	static FSoftObjectPath GetBackpack(const FGSTN_LoadoutData& Loadout);

	/**
	 * Puts Backpack on (spec §10: one at a time, picking up another drops the current one). OutNew is Current with the new backpack,
	 * OutPrevious the one it replaces (empty when none), which the caller drops. False with a reason for an empty backpack or the one
	 * already carried. The other loadout fields and extensions are untouched
	 */
	static bool Equip(const FGSTN_LoadoutData& Current, const FSoftObjectPath& Backpack, FGSTN_LoadoutData& OutNew, FSoftObjectPath& OutPrevious, TArray<FString>& OutErrors);

	/** Takes the backpack off (a drop). OutRemoved is what it carried, empty when nothing */
	static void Remove(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FSoftObjectPath& OutRemoved);
};
