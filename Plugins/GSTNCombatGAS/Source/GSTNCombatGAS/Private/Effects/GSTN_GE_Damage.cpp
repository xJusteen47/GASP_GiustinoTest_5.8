// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_Damage.h"
#include "Effects/GSTN_DamageExecution.h"

UGSTN_GE_Damage::UGSTN_GE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Damage;
	Damage.CalculationClass = UGSTN_DamageExecution::StaticClass();
	Executions.Add(Damage);
}
