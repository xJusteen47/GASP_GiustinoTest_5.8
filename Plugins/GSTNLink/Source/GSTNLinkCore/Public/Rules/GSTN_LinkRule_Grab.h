// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_GrabRules.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_LinkRule_Grab.generated.h"

/**
 * The grab's request rule (m6 spec 8, Grabbability). A grab is accepted only if the target passes, in this order: the shared checks,
 * the tag requirements (including the states of other plugins, by name), the parts gate (Combat.Part.GrabLocked, by name), the weight
 * limit, and the immunity frame.
 *
 * The limits are the grabber's kit data when the context carries it, and this rule's own copy otherwise, so how heavy a target a kit
 * can grab is the kit's. Bosses carry Combat.Immunity.Grab (in the default requirements) and are never grabbable.
 */
UCLASS(meta = (DisplayName = "Grab Rule"))
class GSTNLINKCORE_API UGSTN_LinkRule_Grab : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	/** The rules used when the initiator's kit carries none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab")
	FGSTN_GrabRules Rules;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;
};
