// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_LinkPresentationComponent.generated.h"

class USceneComponent;
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
 *    offset), so it follows the animated hand. The capsule is NOT attached: it stays at the parked transform of the simulation.
 *  - It also holds the collision side of a link for its owner (F15): SetCollisionIgnoredPartner, called by the link tick extension every
 *    simulated frame from the simulated link fragment (FGSTN_LinkExitCollision), never from what this machine shows. Until F15 this tick
 *    set and restored the pair's collision from the shown state, so a client restored it later than the authority (HANDOVER 61 OPEN 2).
 * Camera and aim consumers read GetLinkedTransform() (the project camera change approved in plan section 6 is the consumer's code; this
 * plugin exposes the transform and the bIsLinkedParked flag only).
 */
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
	/** Recomputes the linked transform from the shown state; true while parked */
	bool UpdateLinkedTransform();
	void AttachMesh(AActor* NewPartner);
	void DetachMesh();

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
};
