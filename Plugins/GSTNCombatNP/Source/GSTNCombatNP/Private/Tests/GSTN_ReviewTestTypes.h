// Ability classes used only by the review-fix automation tests (refused commit, montage retention).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_ReviewTestTypes.generated.h"

/** An ability that stays active until told otherwise and whose commit succeeds or is refused as the test chooses */
UCLASS(Hidden)
class UGSTN_TestCommitAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_TestCommitAbility()
	{
		InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	}

	/** Read from the class default object: the instance does not need to be set up by the test */
	bool bRefuseCommit = false;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override
	{
		// Stays active: the test ends it through the commit path
	}

	virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override
	{
		return !bRefuseCommit;
	}
};

UCLASS(Hidden)
class UGSTN_TestRefusedCommitAbility : public UGSTN_TestCommitAbility
{
	GENERATED_BODY()

public:
	UGSTN_TestRefusedCommitAbility()
	{
		bRefuseCommit = true;
	}
};

/** A timeline ability with no content of its own: the test fills the inline timeline */
UCLASS(Hidden)
class UGSTN_TestMontageTimelineAbility : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()
};
