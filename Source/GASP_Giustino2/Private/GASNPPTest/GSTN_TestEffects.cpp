// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestEffects.h"
#include "GASNPPTest/GSTN_TestAttributeSet.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Settings/GSTN_CombatGASSettings.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"

namespace GSTNTestTags
{
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Dash, "Test.Cooldown.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Stat_WalkSpeed, "Combat.SetByCaller.Stat.WalkSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Stat_JumpVelocity, "Combat.SetByCaller.Stat.JumpVelocity");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weight, "Combat.SetByCaller.Stat.Weight");
	UE_DEFINE_GAMEPLAY_TAG(Stat_MaxCarryWeight, "Combat.SetByCaller.Stat.MaxCarryWeight");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Acceleration, "Combat.SetByCaller.Stat.Acceleration");
	UE_DEFINE_GAMEPLAY_TAG(Stat_AirControl, "Combat.SetByCaller.Stat.AirControl");
	UE_DEFINE_GAMEPLAY_TAG(Stat_FallGravityScale, "Combat.SetByCaller.Stat.FallGravityScale");
	UE_DEFINE_GAMEPLAY_TAG(Stat_AirJumpCount, "Combat.SetByCaller.Stat.AirJumpCount");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Buff, "Test.Cooldown.Buff");
	UE_DEFINE_GAMEPLAY_TAG(State_Dashing, "Test.State.Dashing");
	UE_DEFINE_GAMEPLAY_TAG(State_Buffed, "Test.State.Buffed");
	UE_DEFINE_GAMEPLAY_TAG(Probe_Event, "Test.Probe.Event");
	UE_DEFINE_GAMEPLAY_TAG(Probe_TagTrigger, "Test.Probe.TagTrigger");
	UE_DEFINE_GAMEPLAY_TAG(Probe_Bulk, "Test.Probe.Bulk");
}

namespace
{
	FGameplayModifierInfo MakeAdditiveModifier(const FGameplayAttribute& Attribute, float Magnitude)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
		return Modifier;
	}

	void GrantTag(UGameplayEffect* Effect, TArray<TObjectPtr<UGameplayEffectComponent>>& Components, const FGameplayTag& Tag)
	{
		UTargetTagsGameplayEffectComponent* TargetTags = Effect->CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
		FInheritedTagContainer Tags;
		Tags.Added.AddTag(Tag);
		TargetTags->SetAndApplyTargetTagChanges(Tags);
		Components.Add(TargetTags);
	}
}

UGSTN_DashMoveCreator::UGSTN_DashMoveCreator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LayeredMoveDefaultParams.MixMode = EMoveMixMode::OverrideVelocity;
	LayeredMoveDefaultParams.FinishVelocityMode = ELayeredMoveFinishVelocityMode::ClampVelocity;
	LayeredMoveDefaultParams.ClampVelocity = 400.f;
}

FEffectMoveContainer UGSTN_DashMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FLayeredMove_LinearVelocity Move;
	Move.Velocity = FVector(DashSpeed, 0.f, 0.f);
	Move.SettingsFlags = static_cast<uint8>(ELayeredMove_ConstantVelocitySettingsFlags::VelocityStartRelative);
	Move.MixMode = LayeredMoveDefaultParams.MixMode;
	Move.Priority = LayeredMoveDefaultParams.Priority;
	Move.FinishVelocitySettings.FinishVelocityMode = LayeredMoveDefaultParams.FinishVelocityMode;
	Move.FinishVelocitySettings.ClampVelocity = LayeredMoveDefaultParams.ClampVelocity;
	Move.DurationMs = FMath::FloorToFloat(GetDurationFromEffectSpec(EffectSpec) * 1000.f);
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_DashCost::UGSTN_GE_DashCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetStaminaAttribute(), -25.f));
}

UGSTN_GE_DashCooldown::UGSTN_GE_DashCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.5f));
	GrantTag(this, GEComponents, GSTNTestTags::Cooldown_Dash);
}

UGSTN_GE_Dash::UGSTN_GE_Dash()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.3f));
	GrantTag(this, GEComponents, GSTNTestTags::State_Dashing);

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_DashMoveCreator>(TEXT("DashMoveCreator"));
	GEComponents.Add(ApplyMove);
}

UGSTN_GE_BuffCooldown::UGSTN_GE_BuffCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.f));
	GrantTag(this, GEComponents, GSTNTestTags::Cooldown_Buff);
}

UGSTN_GE_Buff::UGSTN_GE_Buff()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.f));
	Period = FScalableFloat(0.5f);
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetHealthAttribute(), 5.f));
	GrantTag(this, GEComponents, GSTNTestTags::State_Buffed);
}

UGSTN_GE_StaminaRegen::UGSTN_GE_StaminaRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(0.25f);
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetStaminaAttribute(), 2.f));
}

UGSTN_GE_ProbeTagGrant::UGSTN_GE_ProbeTagGrant()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
	GrantTag(this, GEComponents, GSTNTestTags::Probe_TagTrigger);
}

UGSTN_GE_ProbeCountEvent::UGSTN_GE_ProbeCountEvent()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetProbeEventCountAttribute(), 1.f));
}

UGSTN_GE_ProbeCountTag::UGSTN_GE_ProbeCountTag()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetProbeTagCountAttribute(), 1.f));
}

UGSTN_GE_ProbeBulk::UGSTN_GE_ProbeBulk()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	GrantTag(this, GEComponents, GSTNTestTags::Probe_Bulk);
}

UGSTN_GE_ProbeStatsFast::UGSTN_GE_ProbeStatsFast()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetProbeSpeedScaleAttribute(), 1.f));
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetProbeGravityScaleAttribute(), -0.5f));
	// The real stats modifier follows the combat WalkSpeed stat (600 by default): +600 doubles it
	Modifiers.Add(MakeAdditiveModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), 600.f));
}

UGSTN_GE_ProbeStatsSlow::UGSTN_GE_ProbeStatsSlow()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetProbeSpeedScaleAttribute(), -0.5f));
	Modifiers.Add(MakeAdditiveModifier(UGSTN_TestAttributeSet::GetProbeGravityScaleAttribute(), 1.f));
	Modifiers.Add(MakeAdditiveModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), -300.f));
}

UGSTN_GE_TestPrimaryWeaponStats::UGSTN_GE_TestPrimaryWeaponStats()
{
	Modifiers.Add(MakeAdditiveModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), 300.f));
}

UGSTN_GE_TestSupportWeaponStats::UGSTN_GE_TestSupportWeaponStats()
{
	Modifiers.Add(MakeAdditiveModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), -300.f));
}

namespace
{
	FGameplayModifierInfo MakeSetByCallerOverride(const FGameplayAttribute& Attribute, const FGameplayTag& Key)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = Key;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}

	/** The stats only the archetype kits set, by attribute and set-by-caller key */
	TArray<TPair<FGameplayAttribute, FGameplayTag>> ArchetypeOnlyStats()
	{
		return {
			{ UGSTN_CombatAttributeSet::GetWeightAttribute(), GSTNTestTags::Stat_Weight },
			{ UGSTN_CombatAttributeSet::GetMaxCarryWeightAttribute(), GSTNTestTags::Stat_MaxCarryWeight },
			{ UGSTN_CombatAttributeSet::GetAccelerationAttribute(), GSTNTestTags::Stat_Acceleration },
			{ UGSTN_CombatAttributeSet::GetAirControlAttribute(), GSTNTestTags::Stat_AirControl },
			{ UGSTN_CombatAttributeSet::GetFallGravityScaleAttribute(), GSTNTestTags::Stat_FallGravityScale },
			{ UGSTN_CombatAttributeSet::GetAirJumpCountAttribute(), GSTNTestTags::Stat_AirJumpCount },
		};
	}
}

UGSTN_GE_TestKitInit::UGSTN_GE_TestKitInit()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerOverride(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), GSTNTestTags::Stat_WalkSpeed));
	Modifiers.Add(MakeSetByCallerOverride(UGSTN_CombatAttributeSet::GetJumpVelocityAttribute(), GSTNTestTags::Stat_JumpVelocity));
	// The archetypes' extra stats back to the project's starting values (the settings are loaded with their class default, before this one)
	const TArray<FGSTN_AttributeDefault>& Defaults = UGSTN_CombatGASSettings::Get().AttributeDefaults;
	for (const TPair<FGameplayAttribute, FGameplayTag>& Stat : ArchetypeOnlyStats())
	{
		const FGSTN_AttributeDefault* Default = Defaults.FindByPredicate([&Stat](const FGSTN_AttributeDefault& Entry)
		{
			return Entry.Attribute == Stat.Key.GetUProperty()->GetFName();
		});
		if (Default)
		{
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = Stat.Key;
			Modifier.ModifierOp = EGameplayModOp::Override;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Default->Value));
			Modifiers.Add(Modifier);
		}
	}
}

UGSTN_GE_TestKitArchetypeInit::UGSTN_GE_TestKitArchetypeInit()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerOverride(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), GSTNTestTags::Stat_WalkSpeed));
	Modifiers.Add(MakeSetByCallerOverride(UGSTN_CombatAttributeSet::GetJumpVelocityAttribute(), GSTNTestTags::Stat_JumpVelocity));
	for (const TPair<FGameplayAttribute, FGameplayTag>& Stat : ArchetypeOnlyStats())
	{
		Modifiers.Add(MakeSetByCallerOverride(Stat.Key, Stat.Value));
	}
}
