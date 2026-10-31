// Test only (M6 F14): a link type added from outside GSTNLink, as m6 spec 14 "A new link type" describes it.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_ExtTestCarryRule.generated.h"

/** The carry's tags, declared here and nowhere in GSTNLink: a new link type brings its own type and role tags */
namespace GSTNExtTestLinkTags
{
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Carry);
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Carrier);
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Carried);
}

/**
 * The carry's request rule: the shared checks, then a weight limit. The "extra semantics" of a new link type (spec 14: carry load) are a rule
 * subclass, pure like every rule (it reads the request context and nothing else), so the LinkRequest step runs it on both machines alike.
 */
UCLASS(meta = (DisplayName = "Ext Test Carry Rule"))
class GSTNEXTTESTLINK_API UGSTN_ExtTestCarryRule : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	/** Heaviest target the carry accepts, in the request context's EffectiveWeight units (100 = a standard fighter) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Carry", meta = (ClampMin = "0.0"))
	float MaxCarryWeight = 150.f;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;
};
