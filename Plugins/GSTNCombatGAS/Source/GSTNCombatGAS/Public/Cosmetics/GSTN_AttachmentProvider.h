// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "GSTN_AttachmentProvider.generated.h"

class AActor;
struct FGSTN_ResolvedLoadout;

/**
 * One mesh a provider wants on the pawn (combat-loadout-mods 9): a kit implant on the back, a backpack on the implant's connector,
 * a weapon implant on a hand. Presentation only; the simulation never reads it.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_AttachmentRequest
{
	GENERATED_BODY()

	/**
	 * Socket of the pawn's mesh (Implant_Back, Weapon_Hand_R...), or of the parent piece's mesh with ParentSlot. A standard socket the
	 * pawn's skeleton lacks goes to a bone through the skin component's SocketFallbacks; anything else attaches at the mesh origin
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName Socket;

	/** A static or a skeletal mesh; anything else is ignored. Soft: loaded asynchronously, the old look stays until it is ready */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	TSoftObjectPtr<UObject> Mesh;

	/** Relative to the socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FTransform Offset;

	/**
	 * Name of this piece of the look (Sword, Implant.Back). A timeline visual track shows or hides it by this name, a later request
	 * attaches to it (ParentSlot), replaces it (bReplacesSlot) or recolours it (a request with no mesh). None = an anonymous piece
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName VisualSlot;

	/**
	 * Attach to the piece an earlier request made under this slot instead of the pawn's mesh; Socket is then a socket of that piece's
	 * mesh. A backpack names Implant.Back and the Connector_Backpack socket, so it fits on top of any kit's implant. A slot nobody made
	 * falls back to the pawn's mesh (no implant: the backpack sits on the back socket)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName ParentSlot;

	/** Shown when nothing asks otherwise. A weapon's sword that only some moves show is authored false (fists-only weapons have none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	bool bShownByDefault = true;

	/**
	 * Swap the mesh of the earlier piece with the same VisualSlot instead of adding one (a mod's longer blade, a gilded implant). The
	 * piece keeps its socket and parent unless this request names its own Socket; its colours are kept, this request's win
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	bool bReplacesSlot = false;

	/** Material vector parameters set on every material of the piece (dynamic instances). With no Mesh: recolour the slot only */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Material", meta = (ForceInlineRow))
	TMap<FName, FLinearColor> VectorParameters;

	/** Material scalar parameters, same rules as VectorParameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Material", meta = (ForceInlineRow))
	TMap<FName, float> ScalarParameters;

	bool HasMaterialParameters() const { return !VectorParameters.IsEmpty() || !ScalarParameters.IsEmpty(); }
};

/** What a provider is handed: the pawn and its resolved loadout (null when the pawn has none yet) */
struct GSTNCOMBATGAS_API FGSTN_AttachmentContext
{
	const AActor* Pawn = nullptr;
	const FGSTN_ResolvedLoadout* Resolved = nullptr;
};

/**
 * Something that adds meshes to a pawn's look from its loadout (combat-loadout-mods 11.5): kit implants, backpacks, mod visuals.
 * The skin component asks every registered provider whenever it applies a look, so GSTNCombat never names a backpack or a mod.
 * Must be a pure function of the context: every machine that renders the pawn asks it.
 */
class GSTNCOMBATGAS_API IGSTN_AttachmentProvider
{
public:
	virtual ~IGSTN_AttachmentProvider() = default;

	/** Appends the meshes this provider wants for the context */
	virtual void GatherAttachments(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests) const = 0;
};

/**
 * The registered attachment providers. An optional plugin registers one from its module startup under its own id and removes it
 * at shutdown. Providers run by priority (lower first), then id order (lexical), so the meshes come in the same order everywhere.
 * Game thread only.
 */
struct GSTNCOMBATGAS_API FGSTN_AttachmentProviderRegistry
{
	/** Registers a provider; registering an id again replaces it */
	static void Register(FName Id, TSharedRef<IGSTN_AttachmentProvider> Provider, int32 Priority = 0);

	/** Removes a provider; returns whether it was registered */
	static bool Unregister(FName Id);

	static bool IsRegistered(FName Id);

	/** Every provider's requests for the context, in order */
	static void Gather(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests);
};
