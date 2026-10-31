// Links, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "State/GSTN_LinkState.h"
#include "GSTN_LinkRequirementLibrary.generated.h"

class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
class UGSTN_PrimaryKitDefinition;
struct FGSTN_LinkKitExtension;
struct FGSTN_TagRequirements;

/**
 * The tag requirement checks of a link request (m6 spec 7.0 item 4, 8), shared by every caller that must agree on whether a link may
 * form: the LinkRequest step on the target side (F11), the interact probe's grab candidates (FGSTN_LinkInteractProvider), and an
 * ability's or a UI's early refusal. All of it is data-driven per link type: the definition's RequestRequirements and its rules in
 * order, so a new type (the drill's latch variant) is a definition plus its own rule subclass, never a branch here.
 *
 * Pure: every function reads its arguments only (the registry is content, fixed for a match), so it gives the same answer on every
 * machine and in a resimulation. Combat.Lock.Link is read through the common checks of UGSTN_LinkRule.
 */
UCLASS()
class GSTNLINKGAS_API UGSTN_LinkRequirementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Whether the tags meet a requirement block (all RequireTags, none of IgnoreTags) */
	static bool MeetsRequirements(const FGameplayTagContainer& Tags, const FGSTN_TagRequirements& Requirements);

	/** Whether the entity refuses every link request: it carries Combat.Lock.Link */
	UFUNCTION(BlueprintPure, Category = "Link")
	static bool IsLinkLocked(const FGameplayTagContainer& Tags);

	/**
	 * Decides a request: UGSTN_LinkRule::CheckCommon (definition, self link, busy channels, coexistence, Combat.Lock.Link, the
	 * definition's RequestRequirements), then each rule of the definition in order; the first refusal wins. A definition with no rule
	 * is decided by the common checks alone. Rules run CheckCommon again themselves; it is pure, so the repeat changes nothing
	 */
	static FGSTN_LinkRuleResult EvaluateRequest(const FGSTN_LinkRequestContext& Context);

	/**
	 * Fills Context.Definition from the registry by Context.LinkType, and each side's OtherChannelDefinition from the link type index
	 * that side's other channel holds, so the coexistence check sees the real link of the other channel. A context whose type is not
	 * registered keeps a null definition (EvaluateRequest then refuses NoDefinition)
	 */
	static void ResolveDefinitions(const UGSTN_LinkRegistry& Registry, FGSTN_LinkRequestContext& Context);

	/** Points the context's kit rules at the initiator kit's link extension (grab and latch rules); null leaves the rules' own copies */
	static void ApplyKitRules(const FGSTN_LinkKitExtension* InitiatorKit, FGSTN_LinkRequestContext& Context);

	/** The kit's link extension, or null when the kit carries none (it then links with the rules' defaults) */
	static const FGSTN_LinkKitExtension* FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit);

	/**
	 * The role tags of every registered role that is parked and does not act while linked (the grabbed; not the latcher, which acts,
	 * nor the grabber, whose pummels and throws are abilities). The combat base ability blocks activation on these, so a plugin's new
	 * parked role is blocked without naming it anywhere
	 */
	static void GetActionBlockingRoleTags(const UGSTN_LinkRegistry& Registry, FGameplayTagContainer& OutTags);
};
