// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Events/GSTN_HitEvents.h"
#include "GSTN_CombatTags.h"
#include "GSTN_HitEffectContext.h"

void FGSTN_HitEventPayload::WriteToContext(FGSTN_HitEffectContext& Context) const
{
	Context.Reaction = Reaction;
	Context.bStaggered = bStaggered;
	Context.HurtboxIndex = HurtboxIndex;
	Context.Relationship = Relationship;
	Context.KOReason = KOReason;
}

FGSTN_HitEventPayload FGSTN_HitEventPayload::FromContext(const FGSTN_HitEffectContext& Context)
{
	FGSTN_HitEventPayload Payload;
	Payload.Reaction = Context.Reaction;
	Payload.bStaggered = Context.bStaggered;
	Payload.HurtboxIndex = Context.HurtboxIndex;
	Payload.Relationship = Context.Relationship;
	Payload.KOReason = Context.KOReason;
	return Payload;
}

FGameplayEventData FGSTN_HitEventPayload::MakeEventData(AActor* Attacker, AActor* Victim, const UGSTN_CombatRules* Rules) const
{
	FGameplayEffectContextHandle Handle = FGSTN_HitEffectContext::Make(Attacker, Attacker, Rules);
	WriteToContext(*FGSTN_HitEffectContext::FromHandleMutable(Handle));

	FGameplayEventData EventData;
	EventData.EventTag = GSTNCombatTags::Event_Hit_Received;
	EventData.Instigator = Attacker;
	EventData.Target = Victim;
	EventData.ContextHandle = Handle;
	return EventData;
}

bool FGSTN_HitEventPayload::FromEventData(const FGameplayEventData& EventData, FGSTN_HitEventPayload& OutPayload)
{
	if (const FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandle(EventData.ContextHandle))
	{
		OutPayload = FromContext(*Context);
		return true;
	}
	return false;
}
