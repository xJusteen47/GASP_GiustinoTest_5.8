// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"
#include "GSTN_EquipmentVisual.generated.h"

class UGSTN_PrimaryKitDefinition;
class UGSTN_WeaponDefinition;
class UObject;

/**
 * The standard names of the equipment look (combat-loadout-mods 9). Sockets are on the character skeleton; the skin component maps a
 * missing one to a bone (SocketFallbacks), so placeholder content works on the GASP mannequin before the sockets are authored.
 */
namespace GSTNEquipment
{
	GSTNCOMBATGAS_API extern const FName Socket_ImplantBack;		// Implant_Back
	GSTNCOMBATGAS_API extern const FName Socket_ImplantHandL;		// Implant_Hand_L
	GSTNCOMBATGAS_API extern const FName Socket_ImplantHandR;		// Implant_Hand_R
	GSTNCOMBATGAS_API extern const FName Socket_ImplantFootL;		// Implant_Foot_L
	GSTNCOMBATGAS_API extern const FName Socket_ImplantFootR;		// Implant_Foot_R
	GSTNCOMBATGAS_API extern const FName Socket_WeaponHandR;		// Weapon_Hand_R

	/** The socket every back implant mesh carries; backpacks attach to it (any backpack fits any kit) */
	GSTNCOMBATGAS_API extern const FName Socket_ConnectorBackpack;	// Connector_Backpack

	/** The slot of the kit's back implant: what a backpack names as its ParentSlot */
	GSTNCOMBATGAS_API extern const FName Slot_ImplantBack;			// Implant.Back
}

/**
 * The look a kit, a weapon (and later a mod or a backpack) adds to the pawn: meshes on sockets, mesh swaps and recolours by slot.
 * Presentation only; the simulation never reads it.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_EquipmentVisual
{
	GENERATED_BODY()

	/** In order: a later piece can attach to, replace or recolour an earlier one by its VisualSlot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TArray<FGSTN_AttachmentRequest> Parts;

	bool IsEmpty() const { return Parts.IsEmpty(); }
};

/** Stateless helpers of the equipment look; game thread, no world needed (the tests call them directly) */
struct GSTNCOMBATGAS_API FGSTN_EquipmentVisualUtils
{
	/**
	 * The loadout's own pieces, before the registered providers': the kit's look, then the primary weapon's (its own, else the first
	 * non-empty one along its loaded parent chain). A kit piece on Implant_Back without a slot gets Implant.Back, so the backpack
	 * connector works without the designer naming it. Additional weapons (support) are not shown yet: which weapon is in hand is
	 * simulation state the look does not follow before the visual track.
	 */
	static void GatherLoadoutVisuals(const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* PrimaryWeapon, TArray<FGSTN_AttachmentRequest>& OutRequests);

	/**
	 * Folds requests into the pieces to build, in order. A request with bReplacesSlot swaps the mesh of the last earlier piece with
	 * its slot (adds it when there is none); a request without a mesh recolours every earlier piece with its slot and is dropped (an
	 * anonymous one too); a ParentSlot is kept only when an earlier piece has that slot, else the piece goes on the pawn's mesh.
	 */
	static void Compose(const TArray<FGSTN_AttachmentRequest>& Requests, TArray<FGSTN_AttachmentRequest>& OutPieces);

	/**
	 * The look's own data problems, as warnings: a part without a mesh that recolours nothing, a replacement without a slot, and a
	 * back implant whose mesh lacks the Connector_Backpack socket (the mesh is loaded in the editor; a game checks it only when
	 * already loaded). Owner prefixes the messages.
	 */
	static void Validate(const FGSTN_EquipmentVisual& Visual, const FString& Owner, TArray<FText>& OutWarnings);

	/** Whether Mesh (static or skeletal) has a socket of that name */
	static bool MeshHasSocket(const UObject* Mesh, FName Socket);
};
