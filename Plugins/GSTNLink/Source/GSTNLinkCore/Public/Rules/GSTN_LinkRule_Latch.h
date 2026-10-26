// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_GrabRules.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_LinkRule_Latch.generated.h"

/**
 * The latch's request rule (owner 2026-10-26, plan F5): the mirror of the grab. A latch is accepted only on a target whose effective
 * weight is at least MinLatchWeight, whose tags meet the requirements (Combat.Immunity.Latch keeps a boss from being ridden), and
 * which has no latcher yet: the shared checks refuse a target whose channel is busy, so one latcher per partner holds without a
 * rule of its own. Combat.Part.GrabLocked is read by name and counts only when the rules ask for it.
 */
UCLASS(meta = (DisplayName = "Latch Rule"))
class GSTNLINKCORE_API UGSTN_LinkRule_Latch : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	/** The rules used when the initiator's kit carries none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Latch")
	FGSTN_LatchRules Rules;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;
};
