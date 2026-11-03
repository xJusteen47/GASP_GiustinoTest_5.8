// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_LinkRule_Drill.generated.h"

/**
 * The request rule of the drill latch (Combat.Link.Type.Drill, D9). The tip's hit asks for the link and the target side decides here, so a
 * refusal comes back to the driller as a refused response: the hit does not land and the dive bounces (spin-drill.md 7.6). Runs GSTNLink's
 * shared checks first (one latcher per target through the busy channel, Combat.Lock.Link, the definition's RequestRequirements), then:
 *  - an ally is never drilled (no team, 255, is nobody's ally, so props and neutral targets are);
 *  - a target carrying Combat.Immunity.Drill refuses (Requirements). Until the parts bridge (plan D12) decides drillability by the tip's
 *    penetration against the part's hardness, this tag is how content marks a hittable but not drillable target;
 *  - the target's latch immunity after its last latcher (LatchImmunityEndFrame, set by GSTNLink) is respected when bRespectLatchImmunity.
 * No weight limit: players, fighters and props are all drillable (spin-drill.md 13b). Pure: reads the context only.
 */
UCLASS(meta = (DisplayName = "Drill Rule"))
class GSTNSPINDRILLCORE_API UGSTN_LinkRule_Drill : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	/** Allies refuse the drill (Team). False lets a team drill its own members (a co-op test) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drill")
	bool bRefuseAllies = true;

	/** The target's latch immunity after its last latcher also blocks the drill */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drill")
	bool bRespectLatchImmunity = true;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;
};
