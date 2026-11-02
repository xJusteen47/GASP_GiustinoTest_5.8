// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "State/GSTN_LinkState.h"
#include "UObject/Object.h"
#include "GSTN_LinkRule.generated.h"

/**
 * A per-link-type request rule (m6 spec 7.0 item 4): decides whether the target accepts a link. The definition lists its rules; the
 * LinkRequest step runs them on the target side, in order, and the first refusal wins.
 *
 * ONE Blueprint-implementable entry point, a const view in and a result struct out, pure: a rule reads the context and nothing
 * else (no actor, no component, no world), so it is deterministic for equal inputs on every machine and in a resimulation. A plugin
 * adds a rule by subclassing (M7: a mount rule); the grab and the latch rules of this plugin are subclasses too.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNLINKCORE_API UGSTN_LinkRule : public UObject
{
	GENERATED_BODY()

public:
	/** Accept or refuse the request. The default accepts */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Link")
	FGSTN_LinkRuleResult Evaluate(const FGSTN_LinkRequestContext& Context) const;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const { return FGSTN_LinkRuleResult::Accept(); }

	/**
	 * The link's Payload byte when the request is accepted (M7 G1: a carry load ratio, quantised to 0..255): the type-specific byte both sides
	 * hold for the life of the link and the link effect reads as Payload / 255. Asked only after every rule accepted; the first rule of the
	 * definition that returns a non-zero value sets it. Pure like Evaluate. The default is 0 (no payload)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Link")
	uint8 ComputePayload(const FGSTN_LinkRequestContext& Context) const;

	virtual uint8 ComputePayload_Implementation(const FGSTN_LinkRequestContext& Context) const { return 0; }

	/** Initiator and target have the same real team (M7 G1, for an ally-only rule); no team (255) is nobody's ally */
	UFUNCTION(BlueprintPure, Category = "Link")
	static bool AreAllies(const FGSTN_LinkRequestContext& Context) { return Context.Initiator.IsAllyOf(Context.Target); }

	/**
	 * The checks every link type shares, which the rules of this plugin run first and a plugin's own rule should too: a definition
	 * exists, the two entities differ, both channels are free, the other channel's link coexists, the target is not locked and meets
	 * the definition's RequestRequirements. Blueprint-callable (F15, HANDOVER 61 OPEN 4): a Blueprint rule's Evaluate calls it first on
	 * the context it was given; the context's definition and other-channel pointers are not reflected but travel with the struct's
	 * native copy, so the Blueprint call decides exactly as the native one. A context made in Blueprint has no definition and is refused
	 * with NoDefinition
	 */
	UFUNCTION(BlueprintPure, Category = "Link")
	static FGSTN_LinkRuleResult CheckCommon(const FGSTN_LinkRequestContext& Context);

	/**
	 * Whether Tags holds the tag with this name. The tag is looked up by name each time and an unknown name is false: the optional
	 * plugin that declares it may not be loaded, and this plugin never names it by an extern
	 */
	UFUNCTION(BlueprintPure, Category = "Link")
	static bool ContainsTagNamed(const FGameplayTagContainer& Tags, FName TagName);
};
