// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestCombatAbilities.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GSTN_CombatCoreTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatLockTags.h"
#include "NativeGameplayTags.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "GameFramework/Actor.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverComponent.h"
#include "Simulation/GSTN_CommandSourceInterface.h"

namespace
{
	// Declared here, not in a header: only these abilities grant them, and the harness reads them by name (Test.* is
	// logged by UGSTN_GASNPPTestComponent). They are not part of the project's public API.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Attacking, "Test.State.Attacking",
		"Owned while the test attack ability is active: the harness uses it to see that an attack command started");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Stance, "Test.State.Stance",
		"Owned while the test stance ability is active (held button): the harness uses it to see that the stance command started");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Triggered, "Test.State.Triggered",
		"Owned while the triggered test timeline runs: the harness uses it to see that the adopted timeline started");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Grab, "Test.State.Grab",
		"Owned while the placeholder grab ability (interact tap) is active: the harness reads which interact command fired");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_PickUp, "Test.State.PickUp",
		"Owned while the placeholder pick up ability (interact hold) is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Escape, "Test.State.Escape",
		"Granted by the harness to give the pawn a kit with a special escape dodge: the default ground dodge ignores it, the escape dodge requires it");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_GammaMove, "Test.State.GammaMove", "Owned while the added command of KIT_Test_Gamma runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Special, "Test.State.Special", "Owned while the plain special of KIT_Test_Delta runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_SpecialEx, "Test.State.SpecialEx", "Owned while the EX special of KIT_Test_Delta runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_SuperA, "Test.State.SuperA", "Owned while super A of KIT_Test_Sigma runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_SuperB, "Test.State.SuperB", "Owned while super B of KIT_Test_Sigma runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Windup, "Test.State.Windup", "Owned while the wait-strike timeline of KIT_Test_Omega runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Strike, "Test.State.Strike", "Granted for half a second by the strike of the wait-strike timeline");

	/** Frames the placeholder interact abilities stay active */
	constexpr int32 InteractTestFrames = 10;
}

UGSTN_GA_TestAttack::UGSTN_GA_TestAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Attacking);

	// 20 frames, free from 14, an 8 frame startup override window and a cancel window [8, 14) into the special commands
	Timeline.TotalFrames = 20;
	Timeline.FirstActionableFrame = 14;
	Timeline.OverrideFrames = 8;

	FGSTN_TimelineTrack_CancelWindow Window;
	Window.StartFrame = 8;
	Window.EndFrame = 14;
	Window.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Special));
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Window));
}

UGSTN_GA_TestTriggeredTimeline::UGSTN_GA_TestTriggeredTimeline(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Triggered);

	// Started by an owned tag being added, not by a command
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GSTNTestTags::Probe_TagTrigger.GetTag();
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(Trigger);

	Timeline.TotalFrames = 12;
	Timeline.FirstActionableFrame = 12;
	Timeline.OverrideFrames = 0;
}

UGSTN_GA_TestStance::UGSTN_GA_TestStance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Stance);
	// The point of the stance: while it is held the attack commands are locked
	ActivationOwnedTags.AddTag(GSTNCombatLockTags::Attack);
	bEndOnTriggerRelease = true;
}

void UGSTN_GA_TestStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base captures the activation context (trigger button included) before anything reads it
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const FRotator Aim = GetAimRotation();
	UE_LOG(LogTemp, Display, TEXT("GASNPP_AIM role=%d local=%d yaw=%.2f pitch=%.2f button=%d source=%d"), static_cast<int32>(GetAvatarActorFromActorInfo() ? GetAvatarActorFromActorInfo()->GetLocalRole() : ROLE_None),
		ActorInfo && ActorInfo->IsLocallyControlled() ? 1 : 0, Aim.Yaw, Aim.Pitch, GetActivationContext().Button, static_cast<int32>(GetActivationContext().Source));

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

UGSTN_GA_TestWeaponSwitch::UGSTN_GA_TestWeaponSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GSTNTestTags::Probe_TagTrigger.GetTag();
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(Trigger);

	Timeline.TotalFrames = 12;
	Timeline.FirstActionableFrame = 12;
	Timeline.OverrideFrames = 0;

	FGSTN_TimelineTrack_WeaponSwitch Switch;
	Switch.SwapFrame = 3;
	Switch.Target = EGSTN_SwitchTarget::Next;
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_WeaponSwitch>(Switch));
}

FEffectMoveContainer UGSTN_TestLaunchMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FLayeredMove_Launch Launch;
	Launch.LaunchVelocity = (TargetMoverComponent ? TargetMoverComponent->GetUpDirection() : FVector::UpVector) * LaunchSpeed;
	// From the ground the launch must leave the walking mode: the air mode of the common settings, like the jump impulse does
	if (const UCommonLegacyMovementSettings* CommonSettings = TargetMoverComponent ? TargetMoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>() : nullptr)
	{
		Launch.ForceMovementMode = CommonSettings->AirMovementModeName;
	}
	return FEffectMoveContainer(&Launch);
}

UGSTN_GE_TestLaunch::UGSTN_GE_TestLaunch()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_TestLaunchMoveCreator>(TEXT("LaunchMoveCreator"));
	GEComponents.Add(ApplyMove);
}

UGSTN_GA_TestLaunch::UGSTN_GA_TestLaunch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GSTNTestTags::Probe_TagTrigger.GetTag();
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(Trigger);
}

void UGSTN_GA_TestLaunch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, GetDefault<UGSTN_GE_TestLaunch>(), GetAbilityLevel(Handle, ActorInfo));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGSTN_GA_TestGrab::UGSTN_GA_TestGrab(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Grab);
}

void UGSTN_GA_TestGrab::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	WaitCombatFrames(InteractTestFrames);
}

void UGSTN_GA_TestGrab::OnCombatFramesElapsed()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

UGSTN_GA_TestPickUp::UGSTN_GA_TestPickUp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_Test_State_PickUp);
}

UGSTN_GA_TestEscapeDodge::UGSTN_GA_TestEscapeDodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The special escape: the same dodge, free to start in hitstun and hitstop. Data, not code
	ActivationBlockedTags.Reset();
}

UGSTN_GE_TestHitstun::UGSTN_GE_TestHitstun()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(GSTNCombatTags::State_Hitstun);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}

UGSTN_GE_TestEscapeKit::UGSTN_GE_TestEscapeKit()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(TAG_Test_State_Escape);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}

namespace
{

	FGameplayModifierInfo MeterModifier(const FGameplayAttribute& Attribute, float Amount)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		return Modifier;
	}
}

UGSTN_GE_TestExCost::UGSTN_GE_TestExCost()
{
	Meter = UGSTN_MeterAttributeSet::GetExMeterAttribute();
	Cost.Value = 50.f;
	RebuildModifiers();
}

UGSTN_GE_TestSuperCost::UGSTN_GE_TestSuperCost()
{
	Meter = UGSTN_MeterAttributeSet::GetSuperMeterAttribute();
	Cost.Value = 100.f;
	RebuildModifiers();
}

UGSTN_GE_TestExMeterGrant::UGSTN_GE_TestExMeterGrant()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MeterModifier(UGSTN_MeterAttributeSet::GetExMeterAttribute(), 60.f));
}

UGSTN_GE_TestSuperMeterGrant::UGSTN_GE_TestSuperMeterGrant()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MeterModifier(UGSTN_MeterAttributeSet::GetSuperMeterAttribute(), 100.f));
}

UGSTN_GE_TestStrike::UGSTN_GE_TestStrike()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.5f));
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(TAG_Test_State_Strike);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}

UGSTN_GA_TestGammaMove::UGSTN_GA_TestGammaMove(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_Test_State_GammaMove);
}

UGSTN_GA_TestSpecial::UGSTN_GA_TestSpecial(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_Test_State_Special);
}

UGSTN_GA_TestSpecialEx::UGSTN_GA_TestSpecialEx(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_Test_State_SpecialEx);
	CostGameplayEffectClass = UGSTN_GE_TestExCost::StaticClass();
}

UGSTN_GA_TestSuperA::UGSTN_GA_TestSuperA(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_Test_State_SuperA);
	CostGameplayEffectClass = UGSTN_GE_TestSuperCost::StaticClass();
}

UGSTN_GA_TestSuperB::UGSTN_GA_TestSuperB(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationOwnedTags.Reset();
	ActivationOwnedTags.AddTag(TAG_Test_State_SuperB);
	CostGameplayEffectClass = UGSTN_GE_TestSuperCost::StaticClass();
}

UGSTN_GA_TestWaitStrike::UGSTN_GA_TestWaitStrike(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Windup);
	Timeline.TotalFrames = 60;
	Timeline.FirstActionableFrame = 60;
	Timeline.OverrideFrames = 0;
}

void UGSTN_GA_TestWaitStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	WaitCombatFrames(WaitFrames);
}

void UGSTN_GA_TestWaitStrike::OnCombatFramesElapsed()
{
	ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), GetDefault<UGSTN_GE_TestStrike>(), 1);
}

UGSTN_GA_TestCostGrab::UGSTN_GA_TestCostGrab(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UGSTN_GA_TestCostGrab::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base captures the activation context first, like the stance
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}
