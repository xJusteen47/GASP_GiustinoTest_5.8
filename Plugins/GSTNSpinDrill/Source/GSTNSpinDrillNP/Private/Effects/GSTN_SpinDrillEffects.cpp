// Spin and drill, Network Prediction half.

#include "Effects/GSTN_SpinDrillEffects.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GSTN_SpinDrillTags.h"
#include "Math/GSTN_DrillMath.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "Sim/GSTN_SpinDrillAttach.h"

FEffectMoveContainer UGSTN_AirSpinHopCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FApplyVelocityEffect Effect;
	Effect.bAdditiveVelocity = false;
	if (!TargetMoverComponent)
	{
		return FEffectMoveContainer(&Effect);
	}
	const UGSTN_MoverSimStateTracker* Tracker = UGSTN_MoverSimStateTracker::FindOrAdd(TargetMoverComponent);
	const FGSTN_DrillSimTuning Tuning = FGSTN_SpinDrillAttach::FindOrDefault(TargetMoverComponent->GetOwner());
	Effect.VelocityToApply = UGSTN_DrillMath::ComputeHopVelocity(Tracker->GetVelocity(), Tracker->GetUpDirection(), Tuning.HopSpeed, Tuning.bHopCancelsFall);
	Effect.ForceMovementMode = Tracker->GetMovementModeName();
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_AirSpinHop::UGSTN_GE_AirSpinHop()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_AirSpinHopCreator>(TEXT("HopCreator"));
	GEComponents.Add(ApplyMove);
}

namespace
{
	FGameplayModifierInfo MultiplyBy(const FGameplayAttribute& Attribute, const FGameplayTag& SetByCaller)
	{
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = SetByCaller;
		FGameplayModifierInfo Info;
		Info.Attribute = Attribute;
		Info.ModifierOp = EGameplayModOp::Multiplicitive;
		Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		return Info;
	}
}

UGSTN_GE_AirSpinLowGravity::UGSTN_GE_AirSpinLowGravity()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Native tags: a class default is built when the module loads, possibly before the tag manager has its tree
	const FGameplayTag Gravity = GSTNSpinDrillTags::SetByCaller_SpinDrill_GravityScale.GetTag();
	Modifiers.Add(MultiplyBy(UGSTN_CombatAttributeSet::GetJumpRiseGravityScaleAttribute(), Gravity));
	Modifiers.Add(MultiplyBy(UGSTN_CombatAttributeSet::GetJumpCutGravityScaleAttribute(), Gravity));
	Modifiers.Add(MultiplyBy(UGSTN_CombatAttributeSet::GetFallGravityScaleAttribute(), Gravity));
	Modifiers.Add(MultiplyBy(UGSTN_CombatAttributeSet::GetMaxFallSpeedAttribute(), GSTNSpinDrillTags::SetByCaller_SpinDrill_FallSpeedScale.GetTag()));

	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(GSTNSpinDrillTags::State_SpinLowGravity.GetTag());
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}

bool UGSTN_GE_AirSpinLowGravity::Sync(UAbilitySystemComponent& AbilitySystem, bool bWanted, const FGSTN_DrillSimTuning& Tuning)
{
	const bool bHas = AbilitySystem.HasMatchingGameplayTag(GSTNSpinDrillTags::State_SpinLowGravity);
	if (bHas == bWanted)
	{
		return false;
	}
	// Without the combat stats there is nothing to scale: a pawn without them is left untouched
	if (bWanted && !AbilitySystem.GetSet<UGSTN_CombatAttributeSet>())
	{
		return false;
	}
	const TSubclassOf<UGameplayEffect> Class = UGSTN_GE_AirSpinLowGravity::StaticClass();
	AbilitySystem.RemoveActiveGameplayEffectBySourceEffect(Class, nullptr);
	if (!bWanted)
	{
		return true;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(Class, 1.f, AbilitySystem.MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNSpinDrillTags::SetByCaller_SpinDrill_GravityScale, Tuning.SpinGravityScale);
	Spec.Data->SetSetByCallerMagnitude(GSTNSpinDrillTags::SetByCaller_SpinDrill_FallSpeedScale, Tuning.SpinMaxFallSpeedScale);
	AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}
