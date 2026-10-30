// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "UObject/PrimaryAssetId.h"
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

	/** The two capsules collide while linked. False = collision between them is ignored and restored at the exit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
	bool bCollisionWithPartner = false;

	/** Socket of the partner the parked placement is derived from, by name (a part socket when the parts plugin is present). None = the partner's root */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Placement")
	FName SocketOnPartner = NAME_None;

	/** Offset applied to the partner's socket to get the parked placement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role|Placement")
	FTransform SocketOffset = FTransform::Identity;
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
	 * Body-channel link types this type cannot be formed or kept alongside. Meaningful on Hand-channel types only (a mount allows a
	 * Hand link, so a rider can hold and throw; a grab does not): a Body type lists none
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request", meta = (Categories = "Combat.Link.Type"))
	TArray<FGameplayTag> ForbiddenWithBody;

	/**
	 * The initiator's own machine predicts the link before the response arrives. Grabs do not (hitstop and a predicted flag only);
	 * a mount may
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Request")
	bool bInitiatorPredicts = false;

	/** The link breaks (Timeout) this many frames after it formed. 0 = it never times out; a grab's end comes from the mash instead */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Break", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxFrames = 0;

	/** Colour of the link in debug drawing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Debug")
	FColor DebugColor = FColor::Orange;

	/** Role 1 or 2 (any other index gives role 2's definition clamped), for code that indexes roles as the link state does */
	const FGSTN_LinkRoleDef& GetRoleDef(int32 RoleIndex) const { return RoleIndex == 1 ? InitiatorRole : TargetRole; }

	/** The definition's exit placer, or the default ring placer when it names none. Never null */
	const UGSTN_LinkExitPlacer* GetExitPlacer() const;

	/** Index (1 or 2) of the parked role, or 0 when the type parks nobody */
	int32 GetParkedRoleIndex() const;

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
