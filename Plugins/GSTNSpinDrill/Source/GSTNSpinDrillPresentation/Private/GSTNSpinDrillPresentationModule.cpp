// Spin and drill plugin, module GSTNSpinDrillPresentation.

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_DrillPresentationComponent.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"

/**
 * Registers a loadout extension that gives every pawn whose loadout is compiled the drill presentation component, on every machine that
 * renders (a dedicated server shows nothing and gets none). Presentation only: the component reads the shown drill fragment and never writes
 * simulation state, so whether a machine has it changes nothing in the simulation.
 */
class FGSTNSpinDrillPresentationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.ContributeCompileInput = [](const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
		{
			const UActorComponent* Component = Cast<UActorComponent>(Context.WorldContext);
			AActor* Owner = Component ? Component->GetOwner() : nullptr;
			if (Owner && !Owner->IsNetMode(NM_DedicatedServer))
			{
				UGSTN_DrillPresentationComponent::FindOrAdd(Owner);
			}
		};
		FGSTN_LoadoutExtensionRegistry::Register(GetId(), MoveTemp(Extension));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(GetId());
	}

private:
	static FName GetId()
	{
		static const FName Id(TEXT("GSTNSpinDrillPresentation"));
		return Id;
	}
};

IMPLEMENT_MODULE(FGSTNSpinDrillPresentationModule, GSTNSpinDrillPresentation)
