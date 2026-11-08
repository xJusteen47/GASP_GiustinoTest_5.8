// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "GameplayTagContainer.h"
#include "Sim/GSTN_LinkPartnerWindow.h"
#include "GSTN_LinkPresentationComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UGSTN_CombatSimComponent;
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
 *
 * FP G1, predicted partner presentation: on the machine of the INITIATOR of a partner move (its owner is locally controlled, a client), the
 * partner's authored reaction is shown from the initiator's own predicted fragment instead of one network round trip later (G0b: ~350 ms after
 * the action). A second, early tick (TG_PrePhysics) builds the FGSTN_LinkPartnerWindow of the initiator's active move every frame the fragment
 * records one, publishes the reaction on the PARTNER actor's presentation component (GetPredictedPartnerMove, OnPredictedPartnerMove) and, when the
 * partner's timeline names a montage that is already loaded, plays it on the partner's mesh ahead of the synced copy (the "presentation lead":
 * the instance shows the later of its position and the predicted one). Presentation only: it writes no simulation state and a misprediction
 * snaps to the animation frame the synced state shows (accepted). cvar gstn.Link.PredictPartnerPresentation (default 1) turns it off.
 * Camera and aim consumers read GetLinkedTransform() (the project camera change approved in plan section 6 is the consumer's code; this
 * plugin exposes the transform and the bIsLinkedParked flag only).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnShownLinkRoleChanged, FGameplayTag, OldRoleTag, FGameplayTag, NewRoleTag);

/** What happened to a predicted partner move on this machine (FP G1) */
UENUM(BlueprintType)
enum class EGSTN_LinkPredictedPartnerEvent : uint8
{
	/** The partner's part began (its start frame was reached on the initiator's clock) */
	Started,
	/** The predicted part ran to its authored end */
	Ended,
	/** The initiator's record of the move vanished before the partner's part began, or an abort was seen: the presentation stopped at once */
	Dropped
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnPredictedPartnerMove, EGSTN_LinkPredictedPartnerEvent, Event, int32, MoveId);

class UGSTN_LinkPresentationComponent;

/** The early tick of the predicted partner presentation: it runs after the Network Prediction finalize and before the partner's mesh animates (see TickPredictedPartner) */
struct FGSTN_LinkPredictTickFunction : public FTickFunction
{
	UGSTN_LinkPresentationComponent* Target = nullptr;

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template <>
struct TStructOpsTypeTraits<FGSTN_LinkPredictTickFunction> : public TStructOpsTypeTraitsBase2<FGSTN_LinkPredictTickFunction>
{
	enum { WithCopy = false };
};

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

	/**
	 * FP G1: the predicted reaction the INITIATOR's machine publishes on this (the partner's) component, while it lasts. True from the frame the
	 * partner's part begins on the initiator's clock; MoveId is the action provider's id, TimelineFrame the frame of the partner's timeline
	 * (0 at its start). False when nothing is predicted for the owner on this machine
	 */
	UFUNCTION(BlueprintPure, Category = "Link|Presentation")
	bool GetPredictedPartnerMove(int32& OutMoveId, int32& OutTimelineFrame) const;

	/** Started, Ended and Dropped of the predicted partner move; broadcast on the partner's component and on the initiator's own */
	UPROPERTY(BlueprintAssignable, Category = "Link|Presentation")
	FGSTN_OnPredictedPartnerMove OnPredictedPartnerMove;

	/** How many times each event fired on this component (the network test harness reads them; they never reset) */
	UFUNCTION(BlueprintPure, Category = "Link|Presentation")
	int32 GetPredictedPartnerEventCount(EGSTN_LinkPredictedPartnerEvent Event) const { return PredictedEventCounts[static_cast<int32>(Event)]; }

	/** The component of an actor, created and registered when missing (FGSTN_LinkAttach does it on every machine) */
	static UGSTN_LinkPresentationComponent* FindOrAdd(AActor* Owner);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The early tick function's body (FGSTN_LinkPredictTickFunction) */
	void TickPredictedPartner(float DeltaTime);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;

private:
	// ---- FP G1: predicted partner presentation ----
	enum class EPredictPhase : uint8
	{
		/** Nothing predicted: the one cheap test of the early tick */
		Idle,
		/** A window is live: the partner's part is published and its montage led */
		Playing,
		/** The predicted part ended: the synced copy of the montage is kept stopped until the partner's shown state is past it */
		Suppressing
	};

	/** Starts the window of the fragment's initiator move; false (the move remembered as rejected) when it cannot be predicted */
	bool BeginPredictedWindow(const FGSTN_LinkFragment& Fragment, UGSTN_CombatSimComponent& Combat);
	void TickPlaying(const FGSTN_LinkFragment* Fragment, int32 LocalFrame);
	void TickSuppressing(int32 LocalFrame);
	/** Ends the predicted presentation of the window: Dropped stops the montage at once and goes Idle; Ended blends it out and suppresses the synced copy */
	void FinishPredicted(EGSTN_LinkPredictedPartnerEvent Event, int32 LocalFrame);
	/** Leads the partner's montage instance to the position of TimelineFrame (starts it once, when loaded); stops it at the montage's end */
	void DrivePartnerMontage(int32 TimelineFrame);
	void StopPartnerMontage(float BlendOut);
	void ResetPredicted();
	UAnimInstance* GetPartnerAnimInstance() const;
	void PublishPredicted(bool bActive, int32 MoveId, int32 TimelineFrame);
	void NotifyPredicted(EGSTN_LinkPredictedPartnerEvent Event, int32 MoveId);
	void RemovePartnerTickPrerequisite();

	FGSTN_LinkPredictTickFunction PredictTick;
	EPredictPhase PredictPhase = EPredictPhase::Idle;
	FGSTN_LinkPartnerWindow Window;
	/** The move the window builder or the partner lookup refused, so the same record is not retried every tick */
	int16 RejectedMoveId = 0;
	int32 RejectedFrame = 0;
	TWeakObjectPtr<UGSTN_CombatSimComponent> CachedCombat;
	TWeakObjectPtr<AActor> PredictedPartner;
	TWeakObjectPtr<UGSTN_LinkPresentationComponent> PredictedPartnerComp;
	TWeakObjectPtr<USkeletalMeshComponent> PredictedPartnerMesh;
	float PredictStepSeconds = 0.016f;
	bool bPredictStarted = false;
	bool bAbortSeen = false;
	bool bPartnerSeenPlaying = false;
	int32 PredictEndedFrame = 0;
	/** The partner's montage: loaded asynchronously at the window start, used from the first tick it is in memory (never a synchronous load) */
	TSoftObjectPtr<UAnimMontage> PredictMontagePath;
	TSharedPtr<FStreamableHandle> PredictMontageHandle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PredictMontage;
	FName PredictSection = NAME_None;
	float PredictPlayRate = 1.f;
	float PredictSectionStart = 0.f;
	float PredictMontageEnd = 0.f;
	bool bMontageStarted = false;
	bool bMontageStopped = false;

	/** What this (the partner's) component shows for the predicted move */
	bool bPredictedActive = false;
	int32 PredictedMoveId = 0;
	int32 PredictedTimelineFrame = 0;
	int32 PredictedEventCounts[3] = { 0, 0, 0 };

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
