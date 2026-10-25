// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestCombatAbilities.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GSTN_CombatTags.h"
#include "GSTN_CombatCoreTags.h"
#include "Hits/GSTN_CombatHitTags.h"
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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Planted, "Test.State.Planted",
		"Granted by the harness to make the ground dodge a spot dodge (no take-off): the default ground dodge ignores it, the planted dodge requires it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_FeetProbe, "Test.State.FeetProbe", "Owned while the low attack of the dodge scenarios runs");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_HitMove, "Test.State.HitMove", "Owned while the test hit move (KIT_Test_Hits) runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_GammaMove, "Test.State.GammaMove", "Owned while the added command of KIT_Test_Gamma runs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_JumpSquat, "Test.State.JumpSquat", "Owned from the jump request to the take-off of the squat jump of KIT_Test_Squat");
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

UGSTN_GA_TestHitMove::UGSTN_GA_TestHitMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_HitMove);

	Timeline.TotalFrames = 32;
	Timeline.FirstActionableFrame = 28;
	Timeline.OverrideFrames = 0;

	// A ricochet (forced by the harness' fake outcome) ends the move into a recoil that outlasts it: 40 frames against the 32 of the move
	Timeline.bHasRicochetResponse = true;
	Timeline.RicochetResponse.RecoilFrames = 40;

	// What every hit of the move does: an authored hitstop so the scenarios know the windows (8 frames victim, 6 attacker)
	auto MakeHit = [](float Damage)
	{
		FGSTN_HitData Hit;
		Hit.Damage = Damage;
		Hit.BaseKnockback = 500.f;
		Hit.AngleDeg = 30.f;
		Hit.HitstopMode = EGSTN_HitstopMode::Override;
		Hit.HitstopFramesOverride = 8;
		Hit.AttackerHitstopFramesOverride = 6;
		return Hit;
	};
	auto MakeSphere = [](double Radius, uint8 Group, EGSTN_HitboxSpace Space)
	{
		FGSTN_HitboxSpec Spec;
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(Radius);
		Spec.HitGroup = Group;
		Spec.Space = Space;
		return Spec;
	};

	// Root: a sphere that travels forward over the track, swept so it cannot jump over a thin hurtbox
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 4;
		Track.EndFrame = 8;
		FGSTN_HitboxSpec Spec = MakeSphere(50.0, 0, EGSTN_HitboxSpace::Root);
		FGSTN_HitboxKey From;
		From.Frame = 0;
		From.Offset = FVector(80.0, 0.0, 0.0);
		FGSTN_HitboxKey To;
		To.Frame = 3;
		To.Offset = FVector(170.0, 0.0, 0.0);
		Spec.Keys = { From, To };
		Spec.bSweepBetweenFrames = true;
		Spec.Data = MakeHit(8.f);
		Track.Hitboxes.Add(Spec);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	}
	// BakedSocket: hand_r runs along the left side of the attacker (baked below)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 10;
		Track.EndFrame = 14;
		FGSTN_HitboxSpec Spec = MakeSphere(45.0, 1, EGSTN_HitboxSpace::BakedSocket);
		Spec.Socket = TEXT("hand_r");
		Spec.Data = MakeHit(8.f);
		Track.Hitboxes.Add(Spec);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));

		FGSTN_BakedSocketFrames Baked;
		for (int32 Frame = 0; Frame < Timeline.TotalFrames; ++Frame)
		{
			const double Y = FMath::Clamp(260.0 - 40.0 * (Frame - 10), 140.0, 260.0);
			Baked.Frames.Add(FTransform(FVector(100.0, Y, 0.0)));
		}
		Timeline.BakedSockets.Add(TEXT("hand_r"), Baked);
	}
	// WorldAtMoveStart: a sphere left where the move started
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 16;
		Track.EndFrame = 20;
		FGSTN_HitboxSpec Spec = MakeSphere(60.0, 2, EGSTN_HitboxSpace::WorldAtMoveStart);
		FGSTN_HitboxKey Key;
		Key.Offset = FVector(200.0, -160.0, 0.0);
		Spec.Keys = { Key };
		Spec.Data = MakeHit(8.f);
		Track.Hitboxes.Add(Spec);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	}
	// A spin around the attacker, the same whichever way anyone faces: the trade, the friendly fire and the self variant use it
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 20;
		Track.EndFrame = 24;
		FGSTN_HitboxSpec Spec = MakeSphere(140.0, 3, EGSTN_HitboxSpace::Root);
		Spec.Data = MakeHit(6.f);
		FGSTN_HitVariant AllyVariant;
		AllyVariant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::Damage);
		AllyVariant.Override.Values.Damage = 3.f;
		Spec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Ally.GetTag(), AllyVariant);
		Track.Hitboxes.Add(Spec);
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	}

	// Cancellable into the special commands from frame 6, but only once a hit confirmed the run
	FGSTN_TimelineTrack_CancelWindow Window;
	Window.StartFrame = 6;
	Window.EndFrame = 28;
	Window.Condition = EGSTN_CancelCondition::OnHit;
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

UGSTN_GA_TestPlantedGroundDodge::UGSTN_GA_TestPlantedGroundDodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGSTN_GA_TestPlantedGroundDodge::OnTimelineBegan(int32 StateStartFrame)
{
	// The dodge without its impulse: straight to the base of the timeline, which starts the montage and nothing else
	UGSTN_GA_BaseCombat::OnTimelineBegan(StateStartFrame);
}

UGSTN_GA_TestFeetProbe::UGSTN_GA_TestFeetProbe(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_FeetProbe);

	Timeline.TotalFrames = 160;
	Timeline.FirstActionableFrame = 156;
	Timeline.OverrideFrames = 0;
	Timeline.bHasRicochetResponse = true;
	Timeline.RicochetResponse.RecoilFrames = 40;

	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = 2;
	Track.EndFrame = 150;
	FGSTN_HitboxSpec Spec;
	Spec.Shape.Type = EGSTN_ShapeType::Sphere;
	Spec.Shape.Extent = FVector(20.0);
	Spec.HitGroup = 0;
	Spec.Space = EGSTN_HitboxSpace::Root;
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Key.Offset = FVector(175.0, 0.0, -75.0);
	Spec.Keys = { Key };
	Spec.Data.Damage = 4.f;
	Spec.Data.BaseKnockback = 300.f;
	Spec.Data.AngleDeg = 30.f;
	Spec.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec.Data.HitstopFramesOverride = 8;
	Spec.Data.AttackerHitstopFramesOverride = 6;
	Track.Hitboxes.Add(Spec);
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
}

UGSTN_GE_TestPlanted::UGSTN_GE_TestPlanted()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(TAG_Test_State_Planted);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
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

UGSTN_GE_TestMovementLock::UGSTN_GE_TestMovementLock()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(GSTNCombatTags::Lock_Movement);
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

UGSTN_GA_TestJumpSquat::UGSTN_GA_TestJumpSquat(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bOverrideSquatFrames = true;
	SquatFrames = 6;
	ActivationOwnedTags.AddTag(TAG_Test_State_JumpSquat);
}
