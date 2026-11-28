// Spin and drill, ability-system half.

#include "GSTN_SpinDrillKitExtension.h"

#include "Abilities/GSTN_GA_DrillBore.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_SpinDrillLibrary.h"
#include "GSTN_SpinDrillSettings.h"
#include "GSTN_SpinDrillTags.h"
#include "GSTN_SpinDrillTuning.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Sim/GSTN_SpinDrillAttach.h"

FGSTN_SpinDrillKitExtension::FGSTN_SpinDrillKitExtension()
	: BoreAbility(UGSTN_GA_DrillBore::StaticClass())
{
}

const FGSTN_SpinDrillKitExtension* FGSTN_SpinDrillLoadout::FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit)
{
	return Kit ? Kit->Extensions.Find<FGSTN_SpinDrillKitExtension>() : nullptr;
}

DEFINE_LOG_CATEGORY_STATIC(LogGSTNSpinDrillLoadout, Log, All);

namespace GSTNSpinDrillLoadout
{
	bool IsSpinOrDrillCommand(const FGameplayTag& Command)
	{
		return Command == GSTNSpinDrillTags::Command_Movement_Spin_Clockwise.GetTag() || Command == GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise.GetTag()
			|| Command == GSTNSpinDrillTags::Command_Movement_Spin_Button.GetTag() || Command == GSTNSpinDrillTags::Command_Movement_Drill.GetTag();
	}
}

void FGSTN_SpinDrillLoadout::AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UActorComponent* Component = Cast<UActorComponent>(Context.WorldContext);
	const AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		// A tool or a test compiles without a pawn: nothing to attach to
		return;
	}
	const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
	const FGSTN_SpinDrillKitExtension* Extension = FindKitExtension(Kit);
	const UGSTN_SpinDrillSettings& Settings = UGSTN_SpinDrillSettings::Get();
	const bool bKitBindsSpin = Input.Kit.Bindings.ContainsByPredicate([](const FGSTN_BindingInput& Binding)
	{
		return GSTNSpinDrillLoadout::IsSpinOrDrillCommand(Binding.Command);
	});
	// Owner rule (2026-11-24): the spin is a shared system, every kit asset has it unless the kit says otherwise. Only assets: a kit built in
	// memory by code is a test's, whose table is counted
	const bool bFromSettings = !Extension && !bKitBindsSpin && Settings.bAddSpinToEveryKit && Kit && Kit->IsAsset();
	if (!Extension && !bFromSettings)
	{
		// A kit that binds the spin by hand without the entry plays with the defaults the simulation falls back on (D4 behaviour)
		FGSTN_SpinDrillAttach::Clear(Owner);
		return;
	}
	UE_LOG(LogGSTNSpinDrillLoadout, Log, TEXT("SpinDrill loadout: owner=%s kit=%s extension=%d kitBindsSpin=%d fromSettings=%d"), *Owner->GetName(),
		Kit ? *Kit->GetName() : TEXT("none"), Extension ? 1 : 0, bKitBindsSpin ? 1 : 0, bFromSettings ? 1 : 0);
	if (bFromSettings)
	{
		for (const FGSTN_AbilityBinding& Binding : UGSTN_SpinDrillLibrary::MakeDefaultSpinBindings(Settings.bKeyboardShortcut))
		{
			Input.Kit.Bindings.Add(FGSTN_LoadoutAssembly::MakeBindingInput(Binding));
		}
	}
	const FSoftObjectPath Bore = Extension ? Extension->BoreAbility.ToSoftObjectPath() : FSoftObjectPath(UGSTN_GA_DrillBore::StaticClass());
	if (!Bore.IsNull())
	{
		Input.Kit.TriggeredAbilities.AddUnique(Bore);
	}
	// Loaded here, where the loadout is compiled, never in a tick; the values are copied, so nothing has to keep the asset alive
	const TSoftObjectPtr<UGSTN_SpinDrillTuning> TuningRef = Extension ? Extension->Tuning : Settings.DefaultTuning;
	const UGSTN_SpinDrillTuning* Tuning = TuningRef.IsNull() ? nullptr : TuningRef.LoadSynchronous();
	FGSTN_SpinDrillAttach::Apply(Owner, Tuning
		? FGSTN_DrillSimTuning::FromTuning(Tuning->AirSpin, Tuning->Cancel, Tuning->Dive, Tuning->Bore, Tuning->Launch)
		: FGSTN_DrillSimTuning::MakeDefault());
}

FName FGSTN_SpinDrillLoadout::GetLoadoutExtensionId()
{
	static const FName Id(TEXT("GSTNSpinDrill"));
	return Id;
}
