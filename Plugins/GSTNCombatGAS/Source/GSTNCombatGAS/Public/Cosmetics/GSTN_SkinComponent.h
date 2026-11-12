// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"
#include "GSTN_SkinComponent.generated.h"

class UAnimInstance;
class UGSTN_LoadoutComponent;
class UGSTN_SkinDefinition;
class UMaterialInterface;
class USkeletalMesh;
class USceneComponent;
class USkeletalMeshComponent;
struct FStreamableHandle;

/**
 * Puts the skin of the loadout on a pawn (combat-m3 10). Presentation only: it changes a mesh, material overrides and an
 * animation class, and nothing in the simulation reads it, so it runs on every machine that renders the pawn and not at all on a
 * dedicated server.
 *
 * Put it on the pawn. It follows the loadout component of the pawn's PlayerState: when the pawn is spawned, and whenever the
 * applied loadout changes, it applies the loadout's skin (or the pawn's own look when the skin is empty). A client's pawn can
 * exist before its PlayerState has replicated, so it retries for a short while; a game that wires the PlayerState earlier calls
 * BindToLoadout itself. The pawn's original mesh, materials and animation class are remembered the first time a skin is applied
 * and put back by RestoreOriginal, so changing skins never stacks.
 *
 * The skin asset's content is soft; it is loaded here, once, when the skin is applied.
 *
 * It also composes the equipment look (combat-loadout-mods 9): the kit's and the primary weapon's FGSTN_EquipmentVisual, then what
 * the registered attachment providers want (FGSTN_AttachmentProviderRegistry: backpacks, mod visuals), folded by
 * FGSTN_EquipmentVisualUtils::Compose (slot replacements, recolours, a piece on another piece's socket such as a backpack on the
 * implant's Connector_Backpack). Refreshed on every applied loadout. The meshes load asynchronously and the old pieces stay until the
 * new ones are ready, then are swapped in one go, so a kit swap never stacks two implants nor shows a bare back for a frame.
 */
UCLASS(ClassGroup = "GSTN Combat", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNCOMBATGAS_API UGSTN_SkinComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_SkinComponent();

	/** Follow the PlayerState's loadout component by itself when the component begins play */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	bool bAutoBind = true;

	/** How many times a not-yet-replicated PlayerState is waited for before giving up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin", meta = (ClampMin = "0", EditCondition = "bAutoBind"))
	int32 MaxBindAttempts = 40;

	/** Seconds between two attempts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin", meta = (ClampMin = "0.01", ForceUnits = "s", EditCondition = "bAutoBind"))
	float BindRetryInterval = 0.25f;

	/** Follows Loadout from now on and applies its current skin. Replaces a previous binding */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void BindToLoadout(UGSTN_LoadoutComponent* Loadout);

	/** Looks the PlayerState's loadout component up and binds to it. False if there is none yet */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	bool TryBindToPlayerState();

	/**
	 * Applies Skin to the pawn, starting from its original look. Null or a skin that changes nothing restores the original.
	 * Does nothing on a dedicated server or without a skeletal mesh. Returns true if the pawn now shows the requested look.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	bool ApplySkin(UGSTN_SkinDefinition* Skin);

	/** Puts back the mesh, material overrides and animation class the pawn had before any skin */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void RestoreOriginal();

	/**
	 * A standard socket the pawn's mesh lacks goes to this bone or socket instead (placeholder content on the GASP mannequin before the
	 * Implant_* sockets are authored). Defaults are the UE5 mannequin bones
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Equipment", meta = (ForceInlineRow))
	TMap<FName, FName> SocketFallbacks;

	/**
	 * Asks the loadout and the registered providers again for the bound loadout (or none) and rebuilds the pieces. The meshes load
	 * asynchronously: the current pieces stay until the new ones are ready (already loaded meshes swap at once). A newer refresh
	 * cancels a pending one
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void RefreshAttachments();

	/** Whether a refresh waits for meshes to load (the old look is still shown) */
	UFUNCTION(BlueprintPure, Category = "Combat|Skin")
	bool IsAttachmentLoadPending() const;

	/**
	 * Shows or hides every piece of a visual slot whatever its default (a timeline visual track showing the sword during a move).
	 * Kept across refreshes until cleared
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void SetVisualSlotShown(FName Slot, bool bShown);

	/** Back to each piece's bShownByDefault */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void ClearVisualSlotShown(FName Slot);

	/** The first piece built for a visual slot, null if none */
	UFUNCTION(BlueprintPure, Category = "Combat|Skin")
	USceneComponent* FindVisualSlotComponent(FName Slot) const;

	/** The mesh components the providers' requests made, in request order */
	const TArray<TObjectPtr<USceneComponent>>& GetAttachmentComponents() const { return Attachments; }

	/** The skin shown now, null for the original look */
	UFUNCTION(BlueprintPure, Category = "Combat|Skin")
	UGSTN_SkinDefinition* GetAppliedSkin() const { return AppliedSkin.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The mesh the skin goes on: the Character's mesh, else the owner's first skeletal mesh component */
	virtual USkeletalMeshComponent* GetTargetMesh() const;

private:
	UFUNCTION()
	void HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision);

	void RetryBind();
	void CaptureOriginal(const USkeletalMeshComponent& Mesh);
	void RestoreLook(USkeletalMeshComponent& Mesh);

	TWeakObjectPtr<UGSTN_LoadoutComponent> BoundLoadout;
	TWeakObjectPtr<UGSTN_SkinDefinition> AppliedSkin;
	FTimerHandle RetryHandle;
	int32 AttemptsLeft = 0;

	/** The look before the first skin */
	bool bCaptured = false;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> OriginalMesh;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> OriginalAnimClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalOverrides;

	/** Components this component created for attachment requests; destroyed on refresh and at end of play */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Attachments;

	/** The visual slot of each entry of Attachments */
	TArray<FName> AttachmentSlots;

	/** Slots shown or hidden by SetVisualSlotShown */
	TMap<FName, bool> SlotShownOverrides;

	/** The pending asynchronous load, if any; its completion builds Pending */
	TSharedPtr<FStreamableHandle> PendingLoad;
	TArray<FGSTN_AttachmentRequest> PendingPieces;

	/** Bumped on every refresh and at end of play: a completion of an older refresh is ignored */
	uint32 RefreshSerial = 0;

	void ClearAttachments();
	void CancelPendingLoad();
	void BuildAttachments(const TArray<FGSTN_AttachmentRequest>& Pieces);
	void ApplyPieceVisibility(int32 Index);
	FName ResolveSocket(const USceneComponent& Parent, FName Socket) const;
};
