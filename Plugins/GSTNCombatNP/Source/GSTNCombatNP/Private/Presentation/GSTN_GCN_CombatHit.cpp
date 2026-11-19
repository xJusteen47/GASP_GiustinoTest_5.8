// Combat presentation (CP4 of the Blueprint seams): the gameplay cue of a combat hit.

#include "Presentation/GSTN_GCN_CombatHit.h"

#include "GameFramework/Actor.h"
#include "GSTN_HitEffectContext.h"
#include "Presentation/GSTN_HitPresentationComponent.h"

UGSTN_GCN_CombatHit::UGSTN_GCN_CombatHit()
{
}

FGameplayCueParameters UGSTN_GCN_CombatHit::MakeHitParameters(const AActor* Target, const FGameplayCueParameters& Parameters, bool bPlaceAtHit, bool bFaceAlongHit)
{
	FGameplayCueParameters Out = Parameters;
	const FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandle(Parameters.EffectContext);
	if (!Context)
	{
		return Out;
	}
	// The simulation's cue carries only the context (UAbilitySystemGlobals::InitGameplayCueParameters): the Burst places at Location, so copy it
	if (bPlaceAtHit && Out.Location.IsZero() && !FVector(Context->HitLocation).IsZero())
	{
		Out.Location = Context->HitLocation;
	}
	const AActor* Attacker = Parameters.GetInstigator();
	if (bFaceAlongHit && Out.Normal.IsZero() && Attacker && Target && Attacker != Target)
	{
		Out.Normal = (Target->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
	}
	return Out;
}

bool UGSTN_GCN_CombatHit::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	// The attacker's prediction already played this hit's cue here: the real one is skipped once (the record is used up by the question)
	const AActor* Attacker = Parameters.GetInstigator();
	if (bSkipWhenPredicted && Attacker && Attacker != Target && !UGSTN_HitPresentationComponent::ShouldPlayAuthoritativeCue(Attacker, Target, Parameters))
	{
		return false;
	}
	return Super::OnExecute_Implementation(Target, MakeHitParameters(Target, Parameters, bPlaceAtHit, bFaceAlongHit));
}

bool UGSTN_CombatCueLibrary::GetCombatHitInfo(const AActor* Target, const FGameplayCueParameters& Parameters, FGSTN_CombatHitCueInfo& OutInfo)
{
	OutInfo = FGSTN_CombatHitCueInfo();
	const FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandle(Parameters.EffectContext);
	if (!Context)
	{
		return false;
	}
	OutInfo.Location = Context->HitLocation;
	OutInfo.Attacker = Parameters.GetInstigator();
	OutInfo.Reaction = Context->Reaction;
	OutInfo.bStaggered = Context->bStaggered;
	OutInfo.Relationship = Context->Relationship;
	OutInfo.KOReason = Context->KOReason;
	OutInfo.HurtboxIndex = Context->HurtboxIndex;
	if (!Parameters.Normal.IsZero())
	{
		OutInfo.Direction = Parameters.Normal;
	}
	else if (OutInfo.Attacker && Target && Target != OutInfo.Attacker)
	{
		OutInfo.Direction = (Target->GetActorLocation() - OutInfo.Attacker->GetActorLocation()).GetSafeNormal2D();
	}
	return true;
}
