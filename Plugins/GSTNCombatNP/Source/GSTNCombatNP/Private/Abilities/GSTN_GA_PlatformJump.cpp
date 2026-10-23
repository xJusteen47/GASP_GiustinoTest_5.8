// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_GA_PlatformJump.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Jump/GSTN_JumpTypes.h"
#include "MoverComponent.h"
#include "Simulation/GSTN_CombatSimComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPlatformJump, Log, All);

FEffectMoveContainer UGSTN_PlatformJumpMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FJumpImpulseEffect JumpMove;
	JumpMove.UpwardsSpeed = 0.f;

	// The ability system ticked before Mover in this frame: its attributes are the ones of the frame being simulated
	bool bFound = false;
	if (TargetAbilitySystemComponent)
	{
		JumpMove.UpwardsSpeed = TargetAbilitySystemComponent->GetGameplayAttributeValue(bAirJump ? UGSTN_CombatAttributeSet::GetAirJumpVelocityAttribute() : UGSTN_CombatAttributeSet::GetJumpVelocityAttribute(), bFound);
	}
	if (!bFound && TargetMoverComponent)
	{
		if (const UCommonLegacyMovementSettings* CommonSettings = TargetMoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>())
		{
			JumpMove.UpwardsSpeed = CommonSettings->JumpUpwardsSpeed;
		}
	}
	return FEffectMoveContainer(&JumpMove);
}

UGSTN_GE_PlatformJump::UGSTN_GE_PlatformJump()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_PlatformJumpMoveCreator>(TEXT("JumpMoveCreator"));
	GEComponents.Add(ApplyMove);
}

UGSTN_GE_PlatformAirJump::UGSTN_GE_PlatformAirJump()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_PlatformJumpMoveCreator* Creator = CreateDefaultSubobject<UGSTN_PlatformJumpMoveCreator>(TEXT("AirJumpMoveCreator"));
	Creator->bAirJump = true;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}

UGSTN_GA_PlatformJump::UGSTN_GA_PlatformJump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	JumpEffect = UGSTN_GE_PlatformJump::StaticClass();
	AirJumpEffect = UGSTN_GE_PlatformAirJump::StaticClass();
}

void UGSTN_GA_PlatformJump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The base captures the activation context before anything reads it
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// What the combat tick decided for this frame, from the state of the frame being simulated (the same on every machine)
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	FGSTN_JumpState Jump;
	const bool bHaveState = Combat && Combat->ReadJumpState(Jump);
	const bool bDecided = bHaveState && (Role == EGSTN_PlatformJumpRole::FastFall
		? Jump.bFastFalling
		: Jump.Phase == EGSTN_JumpPhase::Rising && Jump.StartFrame == Combat->GetSimulationFrame());
	if (!bDecided)
	{
		UE_LOG(LogGSTNPlatformJump, Verbose, TEXT("%s: activated without a jump decided by the combat tick (state %d), nothing to do"), *GetName(), bHaveState);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Role == EGSTN_PlatformJumpRole::Jump)
	{
		const TSubclassOf<UGameplayEffect>& Effect = Jump.bTakeoffWasAir ? AirJumpEffect : JumpEffect;
		if (Effect)
		{
			ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, Effect.GetDefaultObject(), GetAbilityLevel(Handle, ActorInfo));
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGSTN_GA_PlatformFastFall::UGSTN_GA_PlatformFastFall(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Role = EGSTN_PlatformJumpRole::FastFall;
}
