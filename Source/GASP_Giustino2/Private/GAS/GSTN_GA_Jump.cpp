// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_GA_Jump.h"
#include "GAS/GSTN_AbilityTags.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

FEffectMoveContainer UGSTN_JumpImpulseMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FJumpImpulseEffect JumpMove;
	JumpMove.UpwardsSpeed = UpwardsSpeed;
	if (JumpMove.UpwardsSpeed <= 0.f && TargetMoverComponent)
	{
		if (const UCommonLegacyMovementSettings* CommonSettings = TargetMoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>())
		{
			JumpMove.UpwardsSpeed = CommonSettings->JumpUpwardsSpeed;
		}
	}
	return FEffectMoveContainer(&JumpMove);
}

UGSTN_GE_Jump::UGSTN_GE_Jump()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_JumpImpulseMoveCreator>(TEXT("JumpMoveCreator"));
	GEComponents.Add(ApplyMove);
}

UGSTN_GA_Jump::UGSTN_GA_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetAssetTags(FGameplayTagContainer(GSTNAbilityTags::Ability_Movement_Jump));
	ActivationBlockedTags.AddTag(GSTNAbilityTags::State_Movement_JumpBlocked);
	// On the wall the wall mode does the (wall) jump
	ActivationBlockedTags.AddTag(GSTNAbilityTags::State_Movement_WallSliding);

	JumpEffect = UGSTN_GE_Jump::StaticClass();
	AddActivationInput(TEXT("/Game/Input/IA_Jump.IA_Jump"));
}

bool UGSTN_GA_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (bRequireGround)
	{
		// The ability system ticks before Mover: this is the movement state at the end of the previous frame
		const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent(ActorInfo));
		if (!CharacterMover || !CharacterMover->CanActorJump())
		{
			return false;
		}
	}
	return true;
}

void UGSTN_GA_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (JumpEffect)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, JumpEffect.GetDefaultObject(), GetAbilityLevel(Handle, ActorInfo));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGSTN_GA_Jump::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	TakeOverMoverJump(ActorInfo);
}

void UGSTN_GA_Jump::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	TakeOverMoverJump(ActorInfo);
}

void UGSTN_GA_Jump::TakeOverMoverJump(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!bTakeOverMoverJump)
	{
		return;
	}
	if (UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent(ActorInfo)))
	{
		CharacterMover->SetHandleJump(false);
	}
}
