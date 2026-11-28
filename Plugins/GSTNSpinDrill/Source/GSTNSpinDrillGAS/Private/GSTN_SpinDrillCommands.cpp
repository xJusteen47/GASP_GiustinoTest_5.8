// Dev command: does this world's pawns' compiled table carry the spin? (owner playtest 2026-11-28: "the spin does not seem active")

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_SpinDrillSettings.h"
#include "GSTN_SpinDrillTags.h"
#include "HAL/IConsoleManager.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Sim/GSTN_SpinDrillAttach.h"

#if !UE_BUILD_SHIPPING
namespace GSTNSpinDrillCommands
{
	/** gstn.SpinDrill.Report: per pawn with a loadout, the kit, the spin rows and timelines of its compiled table, and whether the tuning is attached */
	static FAutoConsoleCommandWithWorld ReportCommand(TEXT("gstn.SpinDrill.Report"),
		TEXT("Log, per pawn, whether its compiled binding table has the spin and drill rows (and the setting that adds them)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UGSTN_SpinDrillSettings& Settings = UGSTN_SpinDrillSettings::Get();
			UE_LOG(LogTemp, Log, TEXT("SpinDrill.Report netmode=%d bAddSpinToEveryKit=%d bKeyboardShortcut=%d"), World ? static_cast<int32>(World->GetNetMode()) : -1,
				Settings.bAddSpinToEveryKit ? 1 : 0, Settings.bKeyboardShortcut ? 1 : 0);
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				const UGSTN_LoadoutComponent* Loadout = It->FindComponentByClass<UGSTN_LoadoutComponent>();
				if (!Loadout)
				{
					continue;
				}
				const UGSTN_PrimaryKitDefinition* Kit = Loadout->GetResolvedKit();
				const FGSTN_CompiledBindingTable& Table = Loadout->GetCompiledTable();
				int32 SpinRows = 0;
				int32 DrillRows = 0;
				for (const FGSTN_CompiledEntry& Entry : Table.Entries)
				{
					SpinRows += Entry.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise) || Entry.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise) ? 1 : 0;
					DrillRows += Entry.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Drill) ? 1 : 0;
				}
				int32 SpinTimelines = 0;
				for (const FGSTN_TimelineRegistryEntry& Entry : Table.Registry)
				{
					SpinTimelines += Entry.Ability.ToString().Contains(TEXT("Spin")) || Entry.Ability.ToString().Contains(TEXT("Drill")) ? 1 : 0;
				}
				UE_LOG(LogTemp, Log, TEXT("SpinDrill.Report pawn=%s kit=%s revision=%d entries=%d spinRows=%d drillRows=%d spinDrillTimelines=%d attached=%d"),
					*It->GetName(), Kit ? *Kit->GetPathName() : TEXT("none"), Loadout->GetRevision(), Table.Entries.Num(), SpinRows, DrillRows, SpinTimelines,
					FGSTN_SpinDrillAttach::Find(*It) ? 1 : 0);
			}
		}));
}
#endif
