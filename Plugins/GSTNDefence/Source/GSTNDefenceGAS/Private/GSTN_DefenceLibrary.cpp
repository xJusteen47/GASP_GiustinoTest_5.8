// Defence, ability-system half (no Network Prediction, no Mover).

#include "GSTN_DefenceLibrary.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "Data/GSTN_DefenceKitExtension.h"
#include "Data/GSTN_DefenceRulesExtension.h"
#include "GSTNDefenceCoreModule.h"
#include "GSTN_CombatRulesComponent.h"
#include "GSTN_DefenceSettings.h"
#include "GSTN_DefenceTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Math/GSTN_DefenceMath.h"
#include "Rules/GSTN_CombatRules.h"

FName UGSTN_DefenceLibrary::GetLoadoutExtensionId()
{
	static const FName Id(TEXT("GSTNDefence"));
	return Id;
}

const FGSTN_DefenceKitExtension* UGSTN_DefenceLibrary::FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit)
{
	return Kit ? Kit->Extensions.Find<FGSTN_DefenceKitExtension>() : nullptr;
}

const UGSTN_DefenceRules* UGSTN_DefenceLibrary::ResolveRules(const FGSTN_DefenceKitExtension* Extension, const UObject* WorldContext)
{
	if (Extension && !Extension->Rules.IsNull())
	{
		if (const UGSTN_DefenceRules* Own = Extension->Rules.LoadSynchronous())
		{
			return Own;
		}
	}
	// The match's rules: the defence entry of the combat rules asset the rules component plays. Find tolerates a null context
	if (UGSTN_CombatRulesComponent* RulesComponent = WorldContext ? UGSTN_CombatRulesComponent::Find(WorldContext) : nullptr)
	{
		if (const UGSTN_CombatRules* Combat = RulesComponent->GetRules())
		{
			for (const FInstancedStruct& Entry : Combat->RulesExtensions)
			{
				const FGSTN_DefenceRulesExtension* Defence = Entry.GetPtr<FGSTN_DefenceRulesExtension>();
				if (Defence && !Defence->Rules.IsNull())
				{
					if (const UGSTN_DefenceRules* ModeRules = Defence->Rules.LoadSynchronous())
					{
						return ModeRules;
					}
				}
			}
		}
	}
	const TSoftObjectPtr<UGSTN_DefenceRules>& Default = UGSTN_DefenceSettings::Get().DefaultRules;
	return Default.IsNull() ? nullptr : Default.LoadSynchronous();
}

bool UGSTN_DefenceLibrary::IsShieldAvailableForKit(const UGSTN_PrimaryKitDefinition* Kit, const UObject* WorldContext)
{
	const FGSTN_DefenceKitExtension* Extension = FindKitExtension(Kit);
	return Extension && UGSTN_DefenceMath::IsShieldAvailable(ResolveRules(Extension, WorldContext), Extension->HasShield());
}

bool UGSTN_DefenceLibrary::CanExecuteShield(const UGSTN_DefenceRules* Rules, bool bKitHasShield, double ShieldValue, bool bBreaking, bool bGrounded, EGSTN_ShieldRefusal& OutReason)
{
	if (!UGSTN_DefenceMath::IsShieldAvailable(Rules, bKitHasShield))
	{
		OutReason = EGSTN_ShieldRefusal::Unavailable;
	}
	else if (ShieldValue <= 0.0)
	{
		OutReason = EGSTN_ShieldRefusal::Empty;
	}
	else if (bBreaking)
	{
		OutReason = EGSTN_ShieldRefusal::Breaking;
	}
	else if (!bGrounded)
	{
		OutReason = EGSTN_ShieldRefusal::NotGrounded;
	}
	else
	{
		OutReason = EGSTN_ShieldRefusal::None;
	}
	return OutReason == EGSTN_ShieldRefusal::None;
}

FGSTN_TagRequirements UGSTN_DefenceLibrary::GetupNeutralRequirements()
{
	FGSTN_TagRequirements Requirements;
	Requirements.RequireTags.AddTag(GSTNDefenceTags::State_Knockdown.GetTag());
	return Requirements;
}

TSubclassOf<UGameplayAbility> UGSTN_DefenceLibrary::ResolveAbilityClass(const TSoftClassPtr<UObject>& SoftClass, const TCHAR* What)
{
	if (SoftClass.IsNull())
	{
		return nullptr;
	}
	UClass* Class = SoftClass.LoadSynchronous();
	if (!Class || !Class->IsChildOf<UGameplayAbility>())
	{
		// A kit extension names a class by path: a typo or a class of another plugin must not become a silent "no ability"
		UE_LOG(LogGSTNDefence, Warning, TEXT("%s names %s, which is not a gameplay ability class; the entry is ignored."), What, *SoftClass.ToString());
		return nullptr;
	}
	return Class;
}

void UGSTN_DefenceLibrary::ResolveTechSet(const FGSTN_TechSet& Tech, TMap<EGSTN_TechType, TSubclassOf<UGameplayAbility>>& OutAbilities)
{
	OutAbilities.Reset();
	for (const TPair<EGSTN_TechType, TSoftClassPtr<UObject>>& Pair : Tech.Abilities)
	{
		if (Pair.Key == EGSTN_TechType::None)
		{
			continue;
		}
		if (const TSubclassOf<UGameplayAbility> Class = ResolveAbilityClass(Pair.Value, TEXT("The tech set")))
		{
			OutAbilities.Add(Pair.Key, Class);
		}
	}
}

FGSTN_ResolvedGetupSet UGSTN_DefenceLibrary::ResolveGetupSet(const FGSTN_GetupSet& Getup)
{
	FGSTN_ResolvedGetupSet Resolved;
	Resolved.GroundBounce = ResolveAbilityClass(Getup.GroundBounce, TEXT("The get-up set's ground bounce"));
	Resolved.Roll = ResolveAbilityClass(Getup.Roll, TEXT("The get-up set's roll"));
	Resolved.Neutral = ResolveAbilityClass(Getup.Neutral, TEXT("The get-up set's neutral get-up"));
	for (const TSoftClassPtr<UObject>& Attack : Getup.Attacks)
	{
		if (const TSubclassOf<UGameplayAbility> Class = ResolveAbilityClass(Attack, TEXT("A get-up attack")))
		{
			Resolved.Attacks.Add(Class);
		}
	}
	return Resolved;
}

FGSTN_BindingInput UGSTN_DefenceLibrary::MakeShieldBindingInput(const FSoftObjectPath& ShieldAbility)
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FGSTN_BindingInput Binding;
	Binding.Command = GSTNDefenceTags::Command_Defence_Shield.GetTag();
	Binding.Requirements.RequireTags.AddTag(GSTNInputTags::Context_Grounded.GetTag());
	Binding.Ability = ShieldAbility;

	// Hold(Shield, 0) anchored by the press: the press is the anchor the matcher looks for, the hold with no minimum says the
	// button is still down at that frame, which is what a raise means (a tap still raises; the hold window then ends it)
	FGSTN_InputCondition_Press Press;
	Press.Buttons.AddTag(GSTNCombatButtonTags::Button_Shield.GetTag());
	FGSTN_InputCondition_Hold Hold;
	Hold.Button = GSTNCombatButtonTags::Button_Shield.GetTag();
	Hold.MinFrames = 0;

	FGSTN_InputStep Step;
	Step.AllOf.Add(FCondition::Make<FGSTN_InputCondition_Press>(Press));
	Step.AllOf.Add(FCondition::Make<FGSTN_InputCondition_Hold>(Hold));

	Binding.bHasSlotOverride = true;
	Binding.SlotOverride.Command = Binding.Command;
	Binding.SlotOverride.Pattern.Steps.Add(MoveTemp(Step));
	return Binding;
}

FGSTN_BindingInput UGSTN_DefenceLibrary::MakeGetupNeutralBindingInput(const FSoftObjectPath& NeutralAbility)
{
	FGSTN_BindingInput Binding;
	Binding.Command = GSTNCommandTags::Movement_Jump.GetTag();
	Binding.Requirements = GetupNeutralRequirements();
	Binding.Ability = NeutralAbility;
	return Binding;
}

void UGSTN_DefenceLibrary::ContributeToLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
	const FGSTN_DefenceKitExtension* Extension = FindKitExtension(Kit);
	if (!Extension)
	{
		return;
	}

	// The shield binding only where the shield exists (spec 1d): otherwise Input.Button.Shield stays free for the kit's own binding
	const UGSTN_DefenceRules* Rules = ResolveRules(Extension, Context.WorldContext);
	if (UGSTN_DefenceMath::IsShieldAvailable(Rules, Extension->HasShield()))
	{
		if (const TSubclassOf<UGameplayAbility> Shield = ResolveAbilityClass(Extension->ShieldAbility, TEXT("The kit's shield ability")))
		{
			Input.Kit.Bindings.Add(MakeShieldBindingInput(FSoftObjectPath(Shield.Get())));
		}
	}

	// The get-up neutral is the one get-up a command starts; the rest are started Forced by the model and only need to be granted
	const FGSTN_ResolvedGetupSet Getup = ResolveGetupSet(Extension->GetupSet);
	if (Getup.Neutral)
	{
		Input.Kit.Bindings.Add(MakeGetupNeutralBindingInput(FSoftObjectPath(Getup.Neutral.Get())));
	}
	TArray<TSubclassOf<UGameplayAbility>> Forced;
	Forced.Add(Getup.GroundBounce);
	Forced.Add(Getup.Roll);
	Forced.Append(Getup.Attacks);
	TMap<EGSTN_TechType, TSubclassOf<UGameplayAbility>> Tech;
	ResolveTechSet(Extension->Tech, Tech);
	// Key order, so two machines add the tech classes in the same order (the table sorts the granted list anyway)
	TArray<EGSTN_TechType> Kinds;
	Tech.GenerateKeyArray(Kinds);
	Kinds.Sort();
	for (const EGSTN_TechType Kind : Kinds)
	{
		Forced.Add(Tech[Kind]);
	}
	for (const TSubclassOf<UGameplayAbility>& Class : Forced)
	{
		if (Class)
		{
			Input.Kit.TriggeredAbilities.AddUnique(FSoftObjectPath(Class.Get()));
		}
	}
}

void UGSTN_DefenceLibrary::GrantDefence(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem)
{
	const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
	const FGSTN_DefenceKitExtension* Extension = FindKitExtension(Kit);
	const UGSTN_DefenceRules* Rules = Extension ? ResolveRules(Extension, Context.WorldContext) : nullptr;
	if (!Extension || !UGSTN_DefenceMath::IsShieldAvailable(Rules, Extension->HasShield()))
	{
		// A kit whose shield is unavailable gets no set at all: nothing of the shield exists on its ability system
		RevokeDefence(AbilitySystem);
		return;
	}

	if (FindDefenceAttributeSet(&AbilitySystem))
	{
		return;
	}
	UGSTN_DefenceAttributeSet* Set = NewObject<UGSTN_DefenceAttributeSet>(AbilitySystem.GetOwner());
	Set->InitFromRules(Rules->Shield);
	AbilitySystem.AddAttributeSetSubobject(Set);
}

void UGSTN_DefenceLibrary::RevokeDefence(UAbilitySystemComponent& AbilitySystem)
{
	if (const UGSTN_DefenceAttributeSet* Set = FindDefenceAttributeSet(&AbilitySystem))
	{
		AbilitySystem.RemoveSpawnedAttribute(const_cast<UGSTN_DefenceAttributeSet*>(Set));
	}
}

const UGSTN_DefenceAttributeSet* UGSTN_DefenceLibrary::FindDefenceAttributeSet(const UAbilitySystemComponent* AbilitySystem)
{
	return AbilitySystem ? Cast<UGSTN_DefenceAttributeSet>(AbilitySystem->GetAttributeSet(UGSTN_DefenceAttributeSet::StaticClass())) : nullptr;
}
