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

	/** Socket of the pawn's mesh (Implant_Back, Weapon_Hand_R...). None or a socket the mesh lacks attaches at the mesh origin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName Socket;

	/** A static or a skeletal mesh; anything else is ignored. Soft: loaded when the look is applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment", meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
	TSoftObjectPtr<UObject> Mesh;

	/** Relative to the socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FTransform Offset;

	/** Name a timeline visual track shows or hides (Sword); None = always shown. The track itself is M11 G5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName VisualSlot;
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
