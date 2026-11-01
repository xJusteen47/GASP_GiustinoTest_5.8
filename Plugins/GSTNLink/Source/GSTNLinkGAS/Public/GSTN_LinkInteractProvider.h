// Links, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InteractInterfaces.h"
#include "GSTN_InteractMath.h"
#include "State/GSTN_LinkState.h"

class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_GrabRules;
struct FGSTN_InteractMeaningTable;
struct FGSTN_LatchRules;
struct FGSTN_LinkKitExtension;

/** One entity the link provider may offer to the probe: its request view at the end of the previous frame and where it stands */
struct GSTNLINKGAS_API FGSTN_LinkInteractEntity
{
	/** Entity id, tags, effective weight, immunity frames and link states, exactly as the LinkRequest step will see them */
	FGSTN_LinkRequestSide Side;

	/** Anchor of the entity (capsule centre), quantised to whole centimetres by the provider */
	FVector Location = FVector::ZeroVector;

	/** Reach is measured to the surface: the capsule radius */
	int32 RadiusCm = 0;
};

/**
 * One link type the interact button may form, and the meaning the probe projects for it (m6 spec 8, interaction.md 6 item 2). A grab
 * offers Combat.Link.Type.Grab under Interact.Target.Grab; a later type (the drill's latch variant) is one more offer with its own
 * meaning, never code here.
 */
struct GSTNLINKGAS_API FGSTN_LinkInteractOffer
{
	/** The link type's definition (from the registry) */
	const UGSTN_LinkTypeDefinition* Definition = nullptr;

	/** The Interact.Target leaf projected while a candidate of this offer is the focus; it must be in the probe's meaning table */
	FGameplayTag Meaning;

	/** Reach to the candidate's surface for this offer: the kit's InteractReachCm for a grab. 0 = the probe rules' ReachCm */
	float ReachCm = 0.f;

	/** The initiator kit's rules, or null for the rules' own copies (FGSTN_LinkRequestContext) */
	const FGSTN_GrabRules* GrabRules = nullptr;
	const FGSTN_LatchRules* LatchRules = nullptr;
};

/**
 * What the host hands the link provider through FGSTN_InteractQuery::HostContext (the link tick extension of GSTNLinkNP fills it from
 * the end-of-previous-frame snapshot). Plain views, valid for one probe only.
 */
struct GSTNLINKGAS_API FGSTN_LinkInteractSource
{
	/** The HostContext type name the provider checks before reading Data */
	static FName TypeName();

	/** The interacting entity as the initiator of the request */
	FGSTN_LinkRequestSide Interactor;

	/** Every entity that may be linked with, the interactor included or not (it is skipped by id) */
	TConstArrayView<FGSTN_LinkInteractEntity> Entities;

	/** The link types the interact button offers, in priority order: an entity is offered under the first one whose rules accept it */
	TConstArrayView<FGSTN_LinkInteractOffer> Offers;

	/** Resolves the other channel's definition of each side from its link state; null = the caller already filled them */
	const UGSTN_LinkRegistry* Registry = nullptr;

	/** The host context that points at this source; this source must outlive the probe */
	FGSTN_InteractHostContext ToHostContext() const;
};

/**
 * The link plugin's candidate provider of the interact probe (m6 spec 8, revised by Mod.20: interaction.md owns the probe). It offers
 * the entities a link can form with, inside the offer's reach, and PRE-FILTERS them with the link type's own request checks
 * (UGSTN_LinkRequirementLibrary::EvaluateRequest: busy channels, Combat.Lock.Link, Combat.Part.GrabLocked, Combat.Immunity.Grab, the
 * immunity frames, the weight limit), so the probe never focuses, and never projects Interact.Target.Grab on, a target the grab would
 * refuse. The arc, the reach scoring, the tie rules and the one-tag projection stay the probe's.
 *
 * Deterministic: it reads the query and the source only (no actor, no component, no world), on every machine and in a resimulation.
 * Candidate ids are {GSTNInteractProviders::LinkEntities, entity id}; candidates are live for every frame (the snapshot is the stamp).
 */
class GSTNLINKGAS_API FGSTN_LinkInteractProvider : public IGSTN_InteractCandidateProvider
{
public:
	virtual uint8 GetProviderId() const override;

	/** Appends one candidate per entity that some offer accepts. Needs a FGSTN_LinkInteractSource in Query.HostContext and Query.Meanings */
	virtual void GatherCandidates(const FGSTN_InteractQuery& Query, TArray<FGSTN_InteractCandidate>& OutCandidates) const override;

	/**
	 * The pre-filter: the index of the first offer whose request checks accept the entity at Frame, or INDEX_NONE. OutRefusal (when
	 * given) receives the refusal of the last offer tried, for tests and debug draw
	 */
	static int32 FindAcceptedOffer(const FGSTN_LinkInteractSource& Source, const FGSTN_LinkInteractEntity& Entity, int32 Frame,
		FGSTN_LinkRuleResult* OutRefusal = nullptr);

	/** The standard grab offer: the registry's Combat.Link.Type.Grab under Interact.Target.Grab, reach and rules from the kit when given */
	static FGSTN_LinkInteractOffer MakeGrabOffer(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkKitExtension* InitiatorKit);

	/**
	 * Projects the focus as tags: removes every Interact.Target tag from InOutTags, then adds the focus meaning's tag, only when the focus
	 * is actionable (not a ghost, not unactionable). So the entity carries AT MOST ONE Interact.Target tag per frame and each meaning's
	 * binding, which requires its own tag, never competes with another (m6 spec 8)
	 */
	static void ProjectTargetTag(const FGSTN_InteractFocusResult& Focus, const FGSTN_InteractMeaningTable& Meanings, FGameplayTagContainer& InOutTags);
};
