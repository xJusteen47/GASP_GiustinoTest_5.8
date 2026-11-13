// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_LinkTypeDefinition.generated.h"

/**
 * Primary asset types of the link plugin. A link type definition returns one from GetPrimaryAssetId so the asset manager finds
 * every link type, whichever plugin ships it (m6 spec 7.0 item 3); code never lists them. The module registers the type itself over
 * the plugin's content and UGSTN_LinkSettings::LinkTypeDirectories, so the project's DefaultGame.ini does not list it.
 */
namespace GSTNLinkAssetTypes
{
	inline FPrimaryAssetType LinkType() { return FPrimaryAssetType(TEXT("GSTN_LinkType")); }
}

class UGSTN_LinkExitPlacer;
class UGSTN_LinkRule;

/** Limits the wire form of the link state relies on */
namespace GSTNLinkLimits
{
	/** Link types the registry may index: the type index travels as a byte in the link state, and 0 means no link */
	constexpr int32 MaxLinkTypes = 255;
}

/**
 * Which of an entity's two link slots a link type occupies (m6 spec 7.0 item 2). Body links park the entity and are the only ones
 * that enter the linked movement mode; Hand links (a held object) never park and never block the holder's actions by themselves.
 */
UENUM(BlueprintType)
enum class EGSTN_LinkChannel : uint8
{
	Body,
	Hand
};

/**
 * A gameplay effect a role carries while its link is Active (M7 G1, a carry load on the carrier): applied by the link tick extension on the
 * frame the role becomes Active, with the link's Payload / 255 as the SetByCaller magnitude, and removed by its source effect class on every
 * end of the link, whatever the reason. Data only: a path to the effect class, so this module needs no ability system (the class must derive
 * from UGameplayEffect; the NP module loads and checks it). An empty path = no effect.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkEffectDef
{
	GENERATED_BODY()

	/** The effect class (a UGameplayEffect subclass); a duration or infinite effect, removed when the link ends */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (MetaClass = "/Script/GameplayAbilities.GameplayEffect"))
	FSoftClassPath Effect;

	/** The SetByCaller tag the payload magnitude (0..1) is written under. Empty = the effect reads no magnitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FGameplayTag SetByCallerTag;

	bool IsSet() const { return Effect.IsValid(); }
};

/**
 * One of the two roles of a link type. Role 1 is the initiator (the entity whose hit or command formed the link), role 2 the
 * target. Which role is parked, and what the others may still do, is data here, so a grab (target parked), a mount (target
 * parked on the carrier) and a latch (initiator parked on the enemy) are the same code path.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkRoleDef
{
	GENERATED_BODY()

	/** The role tag projected onto the entity while it holds this role */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role", meta = (Categories = "Combat.Link.Role"))
	FGameplayTag RoleTag;

	/** The entity holding this role is parked on the partner's socket: no velocity, no input, no gravity, placement derived from the partner. At most one role per type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
	bool bParked = false;

	/**
	 * The entity keeps acting while linked: no action veto, candidates allowed, movement unchanged (a latcher attacking the enemy it
	 * rides, the ridden enemy walking on). False = the combat tick blocks its actions for the link's duration
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
	bool bActsWhileLinked = false;

	/** The two capsules collide while linked. False (on the parked role) = they ignore each other while linked and for the type's ExitCollisionIgnoreFrames after the exit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
	bool bCollisionWithPartner = false;

	/** Socket of the partner the parked placement is derived from, by name (a part socket when the parts plugin is present). None = the partner's root */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Placement")
	FName SocketOnPartner = NAME_None;

	/** Offset applied to the partner's socket to get the parked placement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Placement")
	FTransform SocketOffset = FTransform::Identity;

	/**
	 * The break reason when this role's own move ends the link (a Release key of a link outcome track on the role's timeline: the grabber's
	 * release, the latcher letting go, the latched enemy shaking the latcher off). A Combat.Link.BreakReason tag, resolved through the registry's
	 * reason table; empty = Release. So a shake-off is the latched role's data, not code that names the latch
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Break", meta = (Categories = "Combat.Link.BreakReason"))
	FGameplayTag ReleaseBreakReason;

	/** The effect the entity holding this role carries while the link is Active (a carry load). None by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Effect")
	FGSTN_LinkEffectDef LinkEffect;

	/**
	 * The arc a parked role that acts may turn through, centred on the forward of its parked placement (M7 G2: a parked role facing where it attacks).
	 * The role's link state carries the turn (FGSTN_LinkState::LocalYawQ), written by the link tick from the stick at the start of each of its
	 * moves and, while it is idle, from the stick; the snapshot composes it into the parked transform, so hurtboxes and hitboxes turn with it.
	 * 360 = free; 0 = the role always faces the placement's forward. Ignored for a role that is not parked or does not act. With no stick
	 * the turn stays where it is, so a role that never pushes the stick keeps the placement's facing
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Placement", meta = (ClampMin = "0.0", ClampMax = "360.0", ForceUnits = "deg"))
	float YawLimitDeg = 360.f;
};

/**
 * How the interact button offers a link type (M7 G1, after the G0 seam audit): the Interact.Target meaning the probe projects while a candidate
 * of this type is the focus, its priority against the other meanings and offers, the reach and which kits look for it at all. A definition
 * with no meaning is never offered by the interact button (it forms through a hit only, or as another type's Alternative). The registry
 * compiles every offered type into one list, so a plugin adds a meaning by data, never by editing the link tick.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkInteractOfferDef
{
	GENERATED_BODY()

	/** The Interact.Target leaf projected for this type (Interact.Target.Grab, or a leaf the type's plugin declares). Empty = not offered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (Categories = "Interact.Target"))
	FGameplayTag Meaning;

	/**
	 * Higher first (0..255, the probe's priority scale): the order in which offers are tried on a candidate (the first whose rules accept it
	 * names the candidate's meaning), and the meaning's default priority in the probe. The grab is 100; INT.5 puts a mount below it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = "0", ClampMax = "255"))
	int32 Priority = 100;

	/** Reach to the candidate's surface. 0 = the probe rules' reach */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ReachCm = 0.f;

	/**
	 * Only an interactor whose kit carries an extension of this struct (or a child) is offered the type: the grab needs FGSTN_LinkKitExtension,
	 * a mount its own kit extension. None = every interactor. A kit that no offer applies to never runs the probe (its result reads interpolated
	 * positions on a client, so it must not reach a pawn that never uses it)
	 */
	UPROPERTY(EditAnywhere, Category = "Interact")
	TObjectPtr<UScriptStruct> RequiredKitExtension;

	/**
	 * The relationships (Combat.Relationship.*, interactor to candidate) this offer applies to; empty = every one. The probe tries a candidate's
	 * offers by priority and the first whose rules accept it names the candidate, so an offer that ignores the relationship shadows every lower offer
	 * for that candidate (an unfiltered grab claimed the allies a mount is for). The relationship is GSTNCombat's default rule on the two teams
	 * (UGSTN_RelationshipResolver::DefaultRule: no team is Neutral), the rule a hit uses when the match rules name no resolver; the probe has no
	 * rules asset, so a custom resolver of the match is not consulted (M7 G5 option B). GSTNLink's grab offer lists Enemy and Neutral
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (Categories = "Combat.Relationship"))
	FGameplayTagContainer Relationships;

	bool IsOffered() const { return Meaning.IsValid(); }

	/** Whether the offer applies to a candidate of this relationship (true for every one when Relationships is empty) */
	bool AppliesTo(const FGameplayTag& Relationship) const { return Relationships.IsEmpty() || Relationships.HasTagExact(Relationship); }
};

/** How hits on one role of a link affect the link (m6 spec 3 and 7.5) */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_RoleHitBreak
{
	GENERATED_BODY()

	/** The role whose entity the rule is about; must be one of the two role tags of the definition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (Categories = "Combat.Link.Role"))
	FGameplayTag RoleTag;

	/** A hit on this role breaks the link only if its knockback reaches this value (cm/s); 0 = any hit that lands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinKnockbackToBreak = 0.f;

	/** Hits from the linked partner break the link too. False = they never do, so a pummel keeps the grab */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bPartnerHitsBreak = false;
};

/** When a link breaks by itself (m6 spec 7.5). The reasons Release, Throw, Escape and Forced come from commands and the mash, not from here */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkBreakRules
{
	GENERATED_BODY()

	/** The partner may be this far from the expected socket before the link breaks (Distance). 0 = never */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistanceCm = 0.f;

	/** Which hits on which role break the link. A role with no entry is never broken by a hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	TArray<FGSTN_RoleHitBreak> HitBreaks;

	/** The link breaks (OwnDeath) when the entity that holds it dies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bBreakOnOwnDeath = true;

	/** The link breaks (PartnerDeath) when the partner dies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bBreakOnPartnerDeath = true;

	/**
	 * The link breaks (PartnerState) while the partner carries any of these state tags, such as hitstun or tumble of the grabber.
	 * State tags of any root, so no Categories filter
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	FGameplayTagContainer PartnerMustNot;
};

/**
 * A link type as data (m6 spec 3, "Links"): the identity tag, the channel, the two roles, the break rules, the request
 * requirements and the exit. A new link type (a mount, a held object, a drill) is a new asset of this class and its tags, with no
 * change in this plugin. The asset manager finds every definition by GSTNLinkAssetTypes::LinkType; the registry of the GAS module
 * indexes them in a stable order, so the simulation reads an index and never a tag or a name.
 *
 * A definition whose tags are not registered (the plugin that declares them is missing) fails validation and is skipped by the
 * registry, instead of crashing.
 */
UCLASS(BlueprintType)
class GSTNLINKCORE_API UGSTN_LinkTypeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Identity of the link type: on this asset, on the hits that form it and on its events */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link", meta = (Categories = "Combat.Link.Type"))
	FGameplayTag LinkType;

	/** The link slot the type occupies on both entities */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link")
	EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;

	/** Role 1: the entity whose hit or command forms the link */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Roles")
	FGSTN_LinkRoleDef InitiatorRole;

	/** Role 2: the entity the link is formed with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Roles")
	FGSTN_LinkRoleDef TargetRole;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Break")
	FGSTN_LinkBreakRules Break;

	/** The target's state tags must satisfy these for the request to be accepted (in addition to the request rules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request")
	FGSTN_TagRequirements RequestRequirements;

	/**
	 * The request rules, run in order on the target side after the shared checks; the first refusal wins. A grab lists a grab rule, a
	 * latch a latch rule, a plugin's type its own subclass. Instanced: each definition tunes its rules without a subclass
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Link|Request")
	TArray<TObjectPtr<UGSTN_LinkRule>> Rules;

	/**
	 * Where the parked entity lands when the link ends. Instanced, so the definition tunes the placer (a ring radius) without a
	 * subclass. Empty = the ring placer with its defaults
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Link|Exit")
	TObjectPtr<UGSTN_LinkExitPlacer> ExitPlacer;

	/**
	 * Frames after the exit during which the two former partners still ignore each other's collision, for a type whose parked role ignores
	 * it while linked (IgnoresPartnerCollision). The window is sync state of both sides (the link fragment), so every machine and every
	 * resimulation sweeps the same way: a client that still shows the released partner parked (interpolated) does not walk into a capsule
	 * the authority has already restored, or the other way round. Default 12: the second correction it removes came 1-7 frames after the
	 * break (HANDOVER 61), plus margin. 0 = collision returns on the exit frame
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Exit", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ExitCollisionIgnoreFrames = 12;

	/**
	 * Body-channel link types this type cannot be formed or kept alongside. Meaningful on Hand-channel types only (a mount allows a
	 * Hand link, so a rider can hold and throw; a grab does not): a Body type lists none
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request", meta = (Categories = "Combat.Link.Type"))
	TArray<FGameplayTag> ForbiddenWithBody;

	/**
	 * Link types tried BEFORE this one when a hit or the interact button asks for this type, in order: the first whose request checks accept
	 * the target forms instead, and this type only when none does. The grab lists the latch, so one tap grabs a light enemy and latches onto
	 * one heavy enough for the latch rule (the rules decide by weight; nothing names either type). An alternative never lists further ones
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request", meta = (Categories = "Combat.Link.Type"))
	TArray<FGameplayTag> Alternatives;

	/**
	 * The initiator's own machine accepts its request optimistically (M7 G1): its predicted Pending link becomes Active, flag Predicted kept,
	 * PendingFrames after the request, unless the target's snapshot entry shows it formed with someone else. The real response confirms it
	 * (flag cleared); a refusal comes back as the authority's state (one rollback). Grabs do not (hitstop and a predicted flag only, then the
	 * response); a mount does
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request")
	bool bInitiatorPredicts = false;

	/**
	 * Frames after the request at which an initiator that predicts accepts by itself (bInitiatorPredicts). 2 = the frame the real response would
	 * arrive on a machine that simulates the target, so the authority and the initiator's machine show the link from the same frame
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request", meta = (ClampMin = "1", ForceUnits = "Frames", EditCondition = "bInitiatorPredicts"))
	int32 PendingFrames = 2;

	/**
	 * The parked initiator places its own exit (M7 G6: a rider's dismount, predicted on the rider's machine). When its Body link gets a
	 * PendingBreak, its link tick runs the exit placer (the playing partner move's, else the type's) around the partner as the initiator's view
	 * shows it (IGSTN_CombatTickServices::GetViewSnapshot: on the authority the partner rewound to the rider's input time) and records the
	 * exit, due the next frame, when the initiator ends the link by itself exactly as the watchdog's command would. The watchdog copies that
	 * exit into its command instead of placing one, unless the place overlaps the world (then its own placement: one correction). The
	 * initiator places without collision. Only for a type whose parked role is the initiator; off = every exit server-given (grabs, latches)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Break")
	bool bInitiatorPlacesExit = false;

	/** How the interact button offers this type. No meaning = only hits (or another type's Alternatives) form it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Interact")
	FGSTN_LinkInteractOfferDef InteractOffer;

	/** The link breaks (Timeout) this many frames after it formed. 0 = it never times out; a grab's end comes from the mash instead */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Break", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxFrames = 0;

	/**
	 * Presentation only (M7 G10): how long the parked role's mesh takes to slide from where it was shown onto the partner's socket when it
	 * is attached, and back onto its own capsule when it is detached (a refused optimistic mount, an exit). 0 = snap, the M6 behaviour.
	 * Never read by the simulation: the capsule and the hurtboxes are on the parked transform from the attach frame on
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0", ForceUnits = "s"))
	float PresentationBlendSeconds = 0.f;

	/** Colour of the link in debug drawing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Debug")
	FColor DebugColor = FColor::Orange;

	/** Role 1 or 2 (any other index gives role 2's definition clamped), for code that indexes roles as the link state does */
	const FGSTN_LinkRoleDef& GetRoleDef(int32 RoleIndex) const { return RoleIndex == 1 ? InitiatorRole : TargetRole; }

	/** The definition's exit placer, or the default ring placer when it names none. Never null */
	const UGSTN_LinkExitPlacer* GetExitPlacer() const;

	/** Index (1 or 2) of the parked role, or 0 when the type parks nobody */
	int32 GetParkedRoleIndex() const;

	/** The two capsules ignore each other while linked: the parked role says bCollisionWithPartner false. A type that parks nobody never does */
	bool IgnoresPartnerCollision() const;

	/** Index (1 or 2) of the role whose tag is RoleTag, or 0 when neither is */
	int32 FindRoleIndex(const FGameplayTag& RoleTag) const;

	/**
	 * Whether a link of this type can exist at the same time as a link of Other on the other channel: two Body links never can, a
	 * Hand link can unless it lists the Body type in ForbiddenWithBody. Symmetric, so the order of the arguments does not matter
	 */
	bool CanCoexistWith(const UGSTN_LinkTypeDefinition& Other) const;

	/** Appends one message per problem; pure, so tests and the editor validator share it. A definition with an error is skipped by the registry */
	void Validate(TArray<FString>& OutErrors) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
