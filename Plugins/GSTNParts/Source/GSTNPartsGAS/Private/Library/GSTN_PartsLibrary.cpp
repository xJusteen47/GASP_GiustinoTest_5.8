// Destructible parts, ability-system half (no Network Prediction, no Mover).

#include "Library/GSTN_PartsLibrary.h"

#include "AbilitySystemGlobals.h"
#include "Data/GSTN_PartRestoreRequest.h"
#include "Effects/GSTN_GEComponent_RestoreParts.h"
#include "GameFramework/Actor.h"

bool UGSTN_PartsLibrary::RequestRestore(AActor* Target, int32 PartMask, float DamageFraction, TSubclassOf<UGameplayEffect> EffectClass, AActor* Instigator)
{
	if (!Target || !Target->HasAuthority())
	{
		return false;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!ASC)
	{
		return false;
	}
	FGSTN_PartRestoreRequest Request;
	Request.PartMask = PartMask;
	Request.DamageFraction = DamageFraction;
	return FGSTN_PartRestoreQueue::Apply(*ASC, EffectClass, Request, Instigator);
}
