#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
struct FGSTN_TransitionRule;

/**
 * Reads and writes the rule graph of a transition or conduit (the K2 graph that drives bCanEnterTransition).
 * Writing resolves and checks every condition first, so a bad rule leaves the graph untouched.
 */
namespace GSTN::AnimRuleGraph
{
	/** The rule as a readable expression ("false" when nothing drives the result). */
	FString DescribeRule(const UEdGraph* RuleGraph);

	/** Replaces the rule of a transition or conduit. False with OutError set when the rule cannot be built. */
	bool WriteRule(UEdGraphNode* TransitionOrConduit, const FGSTN_TransitionRule& Rule, FString& OutExpression, FString& OutError);
}
