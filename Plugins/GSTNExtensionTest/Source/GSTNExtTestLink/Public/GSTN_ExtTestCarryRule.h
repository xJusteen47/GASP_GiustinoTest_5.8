// Test only (M6 F14): a link type added from outside GSTNLink, as m6 spec 14 "A new link type" describes it.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Rules/GSTN_KitExtension.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_ExtTestCarryRule.generated.h"

/** The carry's tags, declared here and nowhere in GSTNLink: a new link type brings its own type and role tags, and its interact meaning (M7 G1) */
namespace GSTNExtTestLinkTags
{
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Carry);
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Carrier);
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Carried);
	GSTNEXTTESTLINK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Target_Carry);
}

/**
 * The carry's own kit data (M7 G1): an entry of a kit's extension array that GSTNLink knows nothing about. The carry rule reads it from the
 * initiator's request side by type (FGSTN_LinkRequestSide::FindKitExtension), and the carry's interact offer is made to kits that carry it only.
 */
USTRUCT()
struct GSTNEXTTESTLINK_API FGSTN_ExtTestCarryKit : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** This kit's capacity, overriding the rule's MaxCarryWeight */
	UPROPERTY(EditAnywhere, Category = "Link|Carry", meta = (ClampMin = "1.0"))
	float MaxCarryWeight = 150.f;
};

/**
 * The carry's request rule: the shared checks, then a weight limit. The "extra semantics" of a new link type (spec 14: carry load) are a rule
 * subclass, pure like every rule (it reads the request context and nothing else), so the LinkRequest step runs it on both machines alike. M7 G1:
 * the limit is the initiator kit's FGSTN_ExtTestCarryKit when it has one, and the rule writes the load ratio (weight / capacity) as the payload.
 */
UCLASS(meta = (DisplayName = "Ext Test Carry Rule"))
class GSTNEXTTESTLINK_API UGSTN_ExtTestCarryRule : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	/** Heaviest target the carry accepts, in the request context's EffectiveWeight units (100 = a standard fighter), for a kit without the carry entry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Carry", meta = (ClampMin = "0.0"))
	float MaxCarryWeight = 150.f;

	/** The capacity this request is decided with: the initiator kit's carry entry, else MaxCarryWeight */
	float GetCapacity(const FGSTN_LinkRequestContext& Context) const;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;

	/** The load ratio, weight / capacity, quantised to 0..255 */
	virtual uint8 ComputePayload_Implementation(const FGSTN_LinkRequestContext& Context) const override;
};
