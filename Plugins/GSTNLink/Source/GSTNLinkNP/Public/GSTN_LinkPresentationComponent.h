// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_LinkPresentationComponent.generated.h"

class UAnimInstance;
class USceneComponent;
struct FStreamableHandle;
class USkeletalMeshComponent;
struct FGSTN_LinkFragment;

/**
 * Where a linked entity is SHOWN on this machine (m6 spec 7.3, "derived transforms"): presentation only, it never writes simulation state.
 * Added by FGSTN_LinkAttach to every attached pawn (or authored on the pawn), on every machine.
 *
 * While the owner holds the PARKED role of its Body link (the combat state this machine shows: simulated, predicted or interpolated):
 *  - GetLinkedTransform() is the partner's transform on THIS machine x the partner's socket of the role x the role's offset: the live socket of
 *    the partner's mesh (presentation only), or the baked one when bUseLiveSocket is off or the mesh has no such socket. On a client that
 *    does not simulate the partner, the partner is interpolated and so is this, which is the intended trade (spike 2.1).
 *  - The owner's skeletal mesh is attached to the partner's mesh at that socket (relative = the mesh's own offset from its root x the role's
 *    offset), so it follows the animated hand. The capsule is NOT attached: it stays at the parked transform of the simulation. The attachment
 *    is asserted on every tick (M7 G1): Mover rewrites its visual component before the actors tick (its base relative transform, and its
 *    smoothing's world transform on the owner's own capsule), which dropped the role offset one frame after the attach and kept a predicting
 *    owner's mesh on its own capsule (S2 findings A and B).
 *  - A type whose initiator predicts (bInitiatorPredicts) and is the parked role is shown parked from its request (Pending) on, so the
 *    optimistic accept does not pop; a refusal detaches it on the frame the state says so.
 *  - M7 G10: the mesh slides onto the socket on attach and back onto its capsule on detach over the type's PresentationBlendSeconds (0 = snap,
 *    the M6 types), from the world transform it was shown at. Presentation only: the capsule and the hurtboxes never blend.
 *  - M7 G10: the role the owner is shown holding (GetShownRoleTag: Active or Exiting, or the predicted Pending of a parked initiator) links
 *    the kit's anim layer for that role (FGSTN_KitLinkAnims, weapon first, UGSTN_LinkAnimSelection) on the owner's mesh, loaded
 *    asynchronously, and unlinks it when the role ends; OnShownRoleChanged fires for other consumers. A role layer must implement its OWN
 *    layer interface (e.g. ALI_LinkRole), not one the anim sets link: unlinking resets the layer nodes to the main blueprint's defaults.
 *  - It also holds the collision side of a link for its owner (F15): SetCollisionIgnoredPartner, called by the link tick extension every
 *    simulated frame from the simulated link fragment (FGSTN_LinkExitCollision), never from what this machine shows. Until F15 this tick
 *    set and restored the pair's collision from the shown state, so a client restored it later than the authority (HANDOVER 61 OPEN 2).
 * Camera and aim consumers read GetLinkedTransform() (the project camera change approved in plan section 6 is the consumer's code; this
 * plugin exposes the transform and the bIsLinkedParked flag only).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnShownLinkRoleChanged, FGameplayTag, OldRoleTag, FGameplayTag, NewRoleTag);

UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNLINKNP_API UGSTN_LinkPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_LinkPresentationComponent();

	/** Follow the partner's animated socket (presentation); false = the baked reference-pose socket the hurtboxes use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link|Presentation")
	bool bUseLiveSocket = true;

	/** Attach the owner's mesh to the partner while parked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link|Presentation")
	bool bAttachMesh = true;

	/** Link the kit's anim layer of the shown role on the owner's mesh (presentation; a dedicated server never loads one) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link|Presentation")
	bool bLinkRoleAnimLayers = true;

	/** The role the owner is shown holding changed on this machine (an empty tag = none) */
	UPROPERTY(BlueprintAssignable, Category = "Link|Presentation")
	FGSTN_OnShownLinkRoleChanged OnShownRoleChanged;

	/** The role tag of the owner's Body link as this machine shows it; empty while it holds none */
	UFUNCTION(BlueprintPure, Category = "Link")
	FGameplayTag GetShownRoleTag() const { return ShownRoleTag; }

	/** 0..1 progress of the mesh's attach or detach slide; 1 when settled (no blend running) */
	UFUNCTION(BlueprintPure, Category = "Link|Presentation")
	float GetPresentationBlendAlpha() const;

	/** The owner's linked transform on this machine; false (Out untouched) while it holds no parked role or the partner is not here */
	UFUNCTION(BlueprintPure, Category = "Link")
	bool GetLinkedTransform(FTransform& OutTransform) const;

	/** The owner is shown parked on a partner this frame */
	UFUNCTION(BlueprintPure, Category = "Link")
	bool IsLinkedParked() const { return bParkedShown; }

	/** The partner actor of the owner's Body link on this machine, or null */
	UFUNCTION(BlueprintPure, Category = "Link")
	AActor* GetBodyPartner() const { return Partner.Get(); }

	/** The link fragment this machine shows for an actor (its combat component's latest state), or null */
	static const FGSTN_LinkFragment* FindShownFragment(const AActor* Actor);

	/**
	 * Makes the owner's root primitive ignore exactly Partner when it moves (null = nobody), undoing the previous one. Only the owner's own
	 * capsule: the partner's tick writes its own. Simulation-driven (FGSTN_LinkExitCollision::Apply), rewritten every simulated frame
	 */
	void SetCollisionIgnoredPartner(AActor* Partner);

	/** The actor the owner's capsule ignores because of a link, or null */
	AActor* GetCollisionIgnoredPartner() const { return IgnoredPartner.Get(); }

	/** The component of an actor, created and registered when missing (FGSTN_LinkAttach does it on every machine) */
	static UGSTN_LinkPresentationComponent* FindOrAdd(AActor* Owner);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Recomputes the linked transform from the shown state; true while parked. OutRoleTag = the shown role (any role, parked or not) */
	bool UpdateLinkedTransform(FGameplayTag& OutRoleTag);
	void AttachMesh(AActor* NewPartner);
	void DetachMesh();
	/** Starts a slide of the mesh from where it is shown now, over the shown type's PresentationBlendSeconds (nothing when 0) */
	void StartMeshBlend(const FTransform& FromWorld);
	/** Applies the running slide over the mesh's settled world transform of this frame; ends it at alpha 1 */
	void ApplyMeshBlend(float DeltaTime);
	/** The shown role tag of this frame; links / unlinks the role anim layer and fires OnShownRoleChanged on a change */
	void UpdateShownRole(const FGameplayTag& NewRoleTag);
	void LinkRoleLayer(const FGameplayTag& RoleTag);
	void UnlinkRoleLayer();
	void HandleRoleLayerLoaded(FGameplayTag ForRoleTag);

	TWeakObjectPtr<AActor> Partner;
	TWeakObjectPtr<AActor> IgnoredPartner;
	TWeakObjectPtr<USceneComponent> MeshOriginalParent;
	FTransform MeshOriginalRelative = FTransform::Identity;
	FName MeshOriginalSocket = NAME_None;
	FTransform LinkedTransform = FTransform::Identity;
	FName PartnerSocket = NAME_None;
	FTransform RoleOffset = FTransform::Identity;
	bool bParkedShown = false;
	bool bMeshAttached = false;

	/** Shown role and its anim layer */
	FGameplayTag ShownRoleTag;
	TSoftClassPtr<UAnimInstance> WantedRoleLayer;
	UPROPERTY(Transient)
	TObjectPtr<UClass> LinkedRoleLayer;
	TSharedPtr<FStreamableHandle> RoleLayerHandle;

	/** Mesh slide: blended from BlendFromWorld to the settled transform over BlendSeconds */
	FTransform BlendFromWorld = FTransform::Identity;
	float BlendSeconds = 0.f;
	float BlendElapsed = 0.f;
	bool bBlending = false;
	/** The blend time of the type shown last (kept after the link ends, for the detach slide) */
	float ShownTypeBlendSeconds = 0.f;
	/** The mesh's world transform at the end of the last tick: where a new slide starts (Mover may have moved the mesh since) */
	FTransform LastShownWorld = FTransform::Identity;
	bool bHasLastShown = false;
};
