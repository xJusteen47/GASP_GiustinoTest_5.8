// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestCombatAbilities.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatLockTags.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CommandSourceInterface.h"

namespace
{
	// Declared here, not in a header: only these abilities grant them, and the harness reads them by name (Test.* is
	// logged by UGSTN_GASNPPTestComponent). They are not part of the project's public API.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Attacking, "Test.State.Attacking",
		"Owned while the test attack ability is active: the harness uses it to see that an attack command started");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Test_State_Stance, "Test.State.Stance",
		"Owned while the test stance ability is active (held button): the harness uses it to see that the stance command started");
}

UGSTN_GA_TestAttack::UGSTN_GA_TestAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationOwnedTags.AddTag(TAG_Test_State_Attacking);

	// 20 frames, free from 14, a 6 frame startup override window and a cancel window [6, 14) into the special commands
	Timeline.TotalFrames = 20;
	Timeline.FirstActionableFrame = 14;
	Timeline.OverrideFrames = 6;

	FGSTN_TimelineTrack_CancelWindow Window;
	Window.StartFrame = 6;
	Window.EndFrame = 14;
	Window.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Special));
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Window));
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
