// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_LinkRule_Team.generated.h"

/** Which team relation a link type requires between initiator and target */
UENUM(BlueprintType)
enum class EGSTN_LinkTeamRequirement : uint8
{
	/** No requirement (the grab: the hit's own target filter decides) */
	Any,
	/** Same real team only (a mount, a carry of a friend). No team (255) is nobody's ally */
	AlliesOnly,
	/** Different teams only, an entity without a team counting as an enemy of everyone */
	EnemiesOnly
};

/**
 * A request rule on the request sides' teams (M7 G1), generic so any link type lists it: the shared checks, then the team relation. On the
 * hit path the hitbox's target filter usually decides the team already; on the interact path the probe's pre-filter runs the rules with no
 * hit, so an ally-only type must list this rule (or check UGSTN_LinkRule::AreAllies in its own) for an enemy in reach not to be offered.
 */
UCLASS(meta = (DisplayName = "Link Team Rule"))
class GSTNLINKCORE_API UGSTN_LinkRule_Team : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Team")
	EGSTN_LinkTeamRequirement Requirement = EGSTN_LinkTeamRequirement::AlliesOnly;

	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;
};
