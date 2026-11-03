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
#include "MoverSimulationTypes.h"
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

FEffectMoveContainer UGSTN_DrillPlungeCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
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
	// Along the entity's own gravity (spec 14 SD.6: gravity only in v1), no steering at the start: the air control steers from here
	Effect.VelocityToApply = UGSTN_DrillMath::ComputeDiveVelocity(-Tracker->GetUpDirection(), FVector::ZeroVector, Tuning.PlungeSpeed, 0.f);
	Effect.ForceMovementMode = Tracker->GetMovementModeName();
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_DrillPlunge::UGSTN_GE_DrillPlunge()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_DrillPlungeCreator>(TEXT("PlungeCreator"));
	GEComponents.Add(ApplyMove);
}

FEffectMoveContainer UGSTN_DrillBounceCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
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
	// The plunge's speed along gravity is replaced by the bounce (a hop that cancels the fall), the rest kept: the Mario stomp feel of 7.6
	Effect.VelocityToApply = UGSTN_DrillMath::ComputeHopVelocity(Tracker->GetVelocity(), Tracker->GetUpDirection(), Tuning.BounceSpeed, true);
	Effect.ForceMovementMode = Tracker->GetMovementModeName();
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_DrillBounce::UGSTN_GE_DrillBounce()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_DrillBounceCreator>(TEXT("BounceCreator"));
	GEComponents.Add(ApplyMove);
}

FEffectMoveContainer UGSTN_DrillLaunchCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
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
	const FVector Up = Tracker->GetUpDirection();
	Effect.VelocityToApply = UGSTN_DrillMath::ComputeLaunchVelocity(Tuning.LaunchDirection, Up, -Up, Tuning.LaunchSpeed);
	Effect.ForceMovementMode = DefaultModeNames::Falling;
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_DrillLaunch::UGSTN_GE_DrillLaunch()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_DrillLaunchCreator>(TEXT("LaunchCreator"));
	GEComponents.Add(ApplyMove);
}

FEffectMoveContainer UGSTN_DrillPopCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
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
	// The parked driller has no speed of its own: the pop is the hop alone
	Effect.VelocityToApply = UGSTN_DrillMath::ComputeHopVelocity(FVector::ZeroVector, Tracker->GetUpDirection(), Tuning.DetachPopSpeed, true);
	Effect.ForceMovementMode = DefaultModeNames::Falling;
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_DrillPop::UGSTN_GE_DrillPop()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_DrillPopCreator>(TEXT("PopCreator"));
	GEComponents.Add(ApplyMove);
}

namespace
{
	FGameplayModifierInfo ScaleBy(const FGameplayAttribute& Attribute, float Scale)
	{
		FGameplayModifierInfo Info;
		Info.Attribute = Attribute;
		Info.ModifierOp = EGameplayModOp::Multiplicitive;
		Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Scale));
		return Info;
	}

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

UGSTN_GE_DrillDiveGravity::UGSTN_GE_DrillDiveGravity()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(ScaleBy(UGSTN_CombatAttributeSet::GetJumpRiseGravityScaleAttribute(), 0.f));
	Modifiers.Add(ScaleBy(UGSTN_CombatAttributeSet::GetJumpCutGravityScaleAttribute(), 0.f));
	Modifiers.Add(ScaleBy(UGSTN_CombatAttributeSet::GetFallGravityScaleAttribute(), 0.f));
	Modifiers.Add(ScaleBy(UGSTN_CombatAttributeSet::GetFastFallGravityScaleAttribute(), 0.f));

	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(GSTNSpinDrillTags::State_DiveGravity.GetTag());
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}

bool UGSTN_GE_DrillDiveGravity::Sync(UAbilitySystemComponent& AbilitySystem, bool bWanted)
{
	const bool bHas = AbilitySystem.HasMatchingGameplayTag(GSTNSpinDrillTags::State_DiveGravity);
	if (bHas == bWanted || (bWanted && !AbilitySystem.GetSet<UGSTN_CombatAttributeSet>()))
	{
		return false;
	}
	const TSubclassOf<UGameplayEffect> Class = UGSTN_GE_DrillDiveGravity::StaticClass();
	AbilitySystem.RemoveActiveGameplayEffectBySourceEffect(Class, nullptr);
	if (bWanted)
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(Class, 1.f, AbilitySystem.MakeEffectContext());
		if (!Spec.IsValid())
		{
			return false;
		}
		AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	return true;
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
