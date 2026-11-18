#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Cosmetics/GSTN_HitstopFeel.h"
#include "MontageSimulator/NetMontageSimulator.h"
#include "Presentation/GSTN_CombatStateLibrary.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Modules/ModuleManager.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_FragmentNetworkVersion.h"

#define LOCTEXT_NAMESPACE "GSTNCombatNP"

namespace
{
	const FName TableLimitsValidatorId(TEXT("GSTN.NP.TableLimits"));

	UNetMontageSimulator* FindMontagePlayer(const AActor* Actor)
	{
		UNpAbilitySystemComponent* Asc = Cast<UNpAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
		return Asc ? Asc->GetMontagePlayer() : nullptr;
	}

	/**
	 * The hitstop presentation of the reaction component (GSTNCombatGAS, Sakurai's hitstop techniques 2026-11-18) reads the hitstop length and
	 * drives the synced montage's presentation lead through these: that module sees neither the combat simulation nor the GAS-NPP fork.
	 * Presentation only, nothing here writes the simulation.
	 */
	void InstallHitstopPresentationHooks()
	{
		FGSTN_HitstopPresentationHooks::GetHitstopSecondsLeft = [](const AActor* Actor) -> float
		{
			if (!UGSTN_CombatStateLibrary::GetCombatComponent(Actor))
			{
				return -1.f;
			}
			const UNpAbilitySystemComponent* Asc = Cast<UNpAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
			const float StepSeconds = (Asc ? Asc->GetFixedStepMs() : 16.f) / 1000.f;
			return static_cast<float>(UGSTN_CombatStateLibrary::GetHitstopFramesLeft(Actor)) * StepSeconds;
		};
		FGSTN_HitstopPresentationHooks::GetPresentedMontageTime = [](const AActor* Actor, const UObject*& OutMontage, float& OutTime) -> bool
		{
			OutMontage = nullptr;
			OutTime = 0.f;
			const UNetMontageSimulator* Player = FindMontagePlayer(Actor);
			const UAnimMontage* Montage = nullptr;
			if (!Player || !Player->GetPresentedMontageTime(Montage, OutTime))
			{
				return false;
			}
			OutMontage = Montage;
			return true;
		};
		FGSTN_HitstopPresentationHooks::SetMontagePresentationLead = [](const AActor* Actor, float LeadSeconds)
		{
			if (UNetMontageSimulator* Player = FindMontagePlayer(Actor))
			{
				Player->SetPresentationLead(LeadSeconds);
			}
		};
	}

	void UninstallHitstopPresentationHooks()
	{
		FGSTN_HitstopPresentationHooks::GetHitstopSecondsLeft = nullptr;
		FGSTN_HitstopPresentationHooks::GetPresentedMontageTime = nullptr;
		FGSTN_HitstopPresentationHooks::SetMontagePresentationLead = nullptr;
	}
}

/**
 * Registers the limits of the combat simulation's table as a loadout validator, so a loadout whose table the simulation could not hold
 * is refused before it reaches it (a binding index is a byte in the sync state's request, a registry index an int16).
 */
class FGSTNCombatNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UGSTN_LoadoutRules::RegisterValidator(TableLimitsValidatorId, &FGSTNCombatNPModule::ValidateTableLimits);
		// A client whose combat sync fragments differ from the server's is refused at connect (FGSTN_FragmentNetworkVersion)
		FGSTN_FragmentNetworkVersion::Install();
		InstallHitstopPresentationHooks();
	}

	virtual void ShutdownModule() override
	{
		UGSTN_LoadoutRules::UnregisterValidator(TableLimitsValidatorId);
		FGSTN_FragmentNetworkVersion::Uninstall();
		UninstallHitstopPresentationHooks();
	}

	static FGSTN_LoadoutRuleResult ValidateTableLimits(const FGSTN_LoadoutRulesView& View)
	{
		FGSTN_LoadoutRuleResult Result;
		if (View.Table.EntryCount > FGSTN_CommandTable::MaxBindings)
		{
			Result.Errors.Add(FText::Format(LOCTEXT("TooManyBindings", "The loadout compiles to {0} bindings, the combat simulation holds at most {1}."),
				View.Table.EntryCount, FGSTN_CommandTable::MaxBindings));
		}
		if (View.Table.RegistryAbilities.Num() > FGSTN_CompiledBindingTable::MaxRegistryEntries)
		{
			Result.Errors.Add(FText::Format(LOCTEXT("TooManyTimelines", "The loadout needs {0} timeline abilities, the combat simulation holds at most {1}."),
				View.Table.RegistryAbilities.Num(), FGSTN_CompiledBindingTable::MaxRegistryEntries));
		}
		return Result;
	}
};

IMPLEMENT_MODULE(FGSTNCombatNPModule, GSTNCombatNP)

#undef LOCTEXT_NAMESPACE
