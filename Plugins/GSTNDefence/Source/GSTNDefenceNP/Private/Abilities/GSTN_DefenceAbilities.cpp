// Defence, Network Prediction half.

#include "Abilities/GSTN_DefenceAbilities.h"
#include "Abilities/GSTN_GA_Shield.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceLibrary.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputTags.h"
#include "Math/GSTN_DefenceMath.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceTickExtension.h"
#include "Simulation/GSTN_CombatSimComponent.h"

UGSTN_GA_Shield::UGSTN_GA_Shield(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The out-of-shield option every kit has: the jump (m6 spec 5.3). Kits add the rest as data (up special, grab, dodge, platform drop)
	FGSTN_OutOfShieldOption Jump;
	Jump.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Movement_Jump.GetTag()));
	ShieldSpec.OutOfShieldOptions.Add(MoveTemp(Jump));
}

void UGSTN_GA_Shield::RebuildTimeline()
{
	FGSTN_ShieldRules Rules;
	Rules.StartupFrames = FMath::Max(StartupFrames, 0);
	Rules.ReleaseFrames = FMath::Max(ReleaseFrames, 0);
	Timeline = ShieldSpec.BuildTimeline(Rules);
}

void UGSTN_GA_Shield::PostInitProperties()
{
	Super::PostInitProperties();
	RebuildTimeline();
}

#if WITH_EDITOR
void UGSTN_GA_Shield::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildTimeline();
}
#endif

bool UGSTN_GA_Shield::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UGSTN_CombatSimComponent* Combat = Avatar ? Avatar->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_DefenceEntityData* Data = FGSTN_DefenceAttach::Find(Avatar);
	if (!AbilitySystem || !Combat || !Data || !Data->Rules)
	{
		// No defence attached: the binding should not exist; refuse as the safeguard the spec asks for
		return false;
	}
	// The shield of the frame: the sync state the combat tick is on (its latest frame), the attribute rates of the end of F-1
	const FGSTN_CombatSyncState* Sync = Combat->GetSyncState();
	const FGSTN_DefenceFragment* Fragment = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Sync->Fragments) : nullptr;
	const FGSTN_DefenceFragment Defaults;
	const FGSTN_DefenceFragment& Frag = Fragment ? *Fragment : Defaults;
	const FGSTN_ShieldRates Rates = FGSTN_DefenceTickExtension::ReadRates(AbilitySystem, *Data->Rules);
	const int32 Frame = Sync ? Sync->Core.Input.History.GetLatestFrame() : 0;
	const double Value = UGSTN_DefenceMath::ShieldValueAt(Frag.MakeShieldView(Rates), FMath::Max(Frame, 0), Rates);
	const bool bGrounded = AbilitySystem->HasMatchingGameplayTag(GSTNInputTags::Context_Grounded);
	EGSTN_ShieldRefusal Reason = EGSTN_ShieldRefusal::None;
	return UGSTN_DefenceLibrary::CanExecuteShield(Data->Rules, Data->bKitHasShield, Value, Frag.IsBreaking(), bGrounded, Reason);
}

UGSTN_GA_Tech::UGSTN_GA_Tech(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGSTN_GA_GroundBounce::UGSTN_GA_GroundBounce(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGSTN_GA_GetupRoll::UGSTN_GA_GetupRoll(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGSTN_GA_GetupNeutral::UGSTN_GA_GetupNeutral(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
