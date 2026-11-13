// Spin and drill, ability-system half.

#include "GSTN_SpinDrillKitExtension.h"

#include "Abilities/GSTN_GA_DrillBore.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
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

void FGSTN_SpinDrillLoadout::AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UActorComponent* Component = Cast<UActorComponent>(Context.WorldContext);
	const AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		// A tool or a test compiles without a pawn: nothing to attach to
		return;
	}
	const FGSTN_SpinDrillKitExtension* Extension = FindKitExtension(Context.Resolved ? Context.Resolved->Kit : nullptr);
	if (!Extension)
	{
		FGSTN_SpinDrillAttach::Clear(Owner);
		return;
	}
	if (!Extension->BoreAbility.IsNull())
	{
		Input.Kit.TriggeredAbilities.AddUnique(Extension->BoreAbility.ToSoftObjectPath());
	}
	// Loaded here, where the loadout is compiled, never in a tick; the values are copied, so nothing has to keep the asset alive
	const UGSTN_SpinDrillTuning* Tuning = Extension->Tuning.IsNull() ? nullptr : Extension->Tuning.LoadSynchronous();
	FGSTN_SpinDrillAttach::Apply(Owner, Tuning
		? FGSTN_DrillSimTuning::FromTuning(Tuning->AirSpin, Tuning->Cancel, Tuning->Dive, Tuning->Bore, Tuning->Launch)
		: FGSTN_DrillSimTuning::MakeDefault());
}

FName FGSTN_SpinDrillLoadout::GetLoadoutExtensionId()
{
	static const FName Id(TEXT("GSTNSpinDrill"));
	return Id;
}
