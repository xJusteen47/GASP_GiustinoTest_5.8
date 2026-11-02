// Spin and drill, ability-system half.

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_SpinDrillKitExtension.generated.h"

class UGSTN_PrimaryKitDefinition;
class UGSTN_SpinDrillTuning;
struct FGSTN_CompileInput;
struct FGSTN_LoadoutExtensionContext;

/**
 * Gives a kit its spin and drill tuning, by data (spin-drill.md 4, 6). An entry of the kit's extension array, so the kit asset never grows a
 * property that names the drill; a combat build without this plugin keeps the entry and ignores it. The spin itself comes from the kit's
 * command bindings (Command.Movement.Spin.* to the spin ability classes); this entry only says which numbers they play with. A kit that binds
 * the spin without the entry plays with the defaults of the tuning structs.
 *
 * The tuning is soft: the loadout extension loads it where the loadout is compiled (host and clients alike) and attaches the values to the
 * pawn (FGSTN_SpinDrillAttach), so the simulation never reads the asset.
 */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLGAS_API FGSTN_SpinDrillKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** The kit's tuning. Empty = the defaults of the tuning structs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpinDrill")
	TSoftObjectPtr<UGSTN_SpinDrillTuning> Tuning;
};

/** The loadout side of the spin and drill: the kit extension resolved into the pawn's attached tuning */
struct GSTNSPINDRILLGAS_API FGSTN_SpinDrillLoadout
{
	/** The kit's entry, null when it has none */
	static const FGSTN_SpinDrillKitExtension* FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit);

	/**
	 * The loadout extension this module registers: attaches the kit's tuning (or the defaults when the entry names none) to the loadout's
	 * owner wherever the loadout is compiled; a kit without the entry detaches, and the pawn plays with the defaults. Adds nothing to the
	 * compile input
	 */
	static void AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	static FName GetLoadoutExtensionId();
};
