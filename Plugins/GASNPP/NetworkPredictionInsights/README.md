# NetworkPredictionInsights (GAS-NPP fork)

Epic's Unreal Insights extension for [NetworkPrediction](../NetworkPrediction/README.md). The `NetworkPrediction` module
traces every simulation frame, input, state, reconcile and rollback on its `NetworkPredictionChannel`
(`NetworkPredictionTrace.h/.cpp`); this plugin analyzes that stream and shows it as a per-actor timeline: for each
simulation, the frames of the autonomous proxy, the authority and the simulated proxies side by side, marked predicted /
repredicted / confirmed / trashed / abandoned, with the content of every input, sync and aux state (the `ToString` of the
state types) and the field that caused each reconcile (`UE_NP_TRACE_RECONCILE`). It is the tool for understanding a
rollback you cannot explain from the logs. It is debugging only: nothing at runtime depends on it.

Status: `NetworkPredictionInsights.uplugin` version `1.0`, by Epic Games, `IsBetaVersion: false`; the window itself
reports "0.1e-32". Copied with the fork and ported to UE 5.8 (`53c8716`); an unmodified copy since.

## Modules

| Module | Type | Loading phase | Contents |
|---|---|---|---|
| `NetworkPredictionInsights` | EditorAndProgram, `ProgramAllowList: UnrealInsights` | Default | `Source/NetworkPredictionInsights/`. Public: `INetworkPredictionProvider.h` (the analysis-side data model: `FSimulationData`, frames, user states, net roles mirrored as `ENP_NetRole` so the plugin does not depend on engine types) and `INetworkPredictionInsightsModule.h` (tab names). Private: `NetworkPredictionTraceModule` (Trace Services module "NetworkPredictionTrace", logger "NetworkPrediction"), `NetworkPredictionAnalyzer` (routes the `NetworkPrediction.*` trace events: `SimulationCreated`, `Tick`, `SimTick`, `InputCmd`, `SyncState`, `AuxState`, `NetRecv`, `ShouldReconcile`, `Reconcile`, `RollbackInject`, `PushInputFrame`, `FixedTickOffset`, `BufferedInput`, `OOBStateMod`, `SystemFault`, `PieBegin`, `Version`...), `NetworkPredictionProvider` (stores them per simulation), `NetworkPredictionInsightsModule` (tab spawner, `-trace=` parsing, auto-connect), and the Slate UI under `UI/` (`SNPWindow`, `SNPToolbar`, `SNPSimFrameView` with the track layout documented at the top of its header, `SNPSimFrameContents`, axis viewports, draw helpers, commands and action manager). |

## Dependencies

- Plugin dependencies: none listed in the `.uplugin`.
- Module dependencies (`NetworkPredictionInsights.Build.cs`), all private: `Core`, `CoreUObject`, `InputCore`,
  `SlateCore`, `Slate`, `TraceLog`, `TraceAnalysis`, `TraceServices`, `TraceInsights`, `TraceInsightsCore`,
  `AssetRegistry`, `ApplicationCore`; `Engine` when the target compiles against the engine; `EditorFramework`,
  `UnrealEd`, `EditorWidgets` in editor targets. It does **not** link `NetworkPrediction`: the two talk only through the
  trace events, which is why the module can also run inside the standalone `UnrealInsights` program.
- Depends on it: nothing. It is not listed in the project `.uproject`; being an editor/program plugin inside the
  project's `Plugins/` tree, it is available to the editor build without an entry.

## Gameplay tags

Declares no gameplay tags.

## How to use it

1. Start the editor (or a game client/server) with `-trace=np` or `-trace=NetworkPrediction`
   (`FNetworkPredictionInsightsModule::StartupModule` accepts both tokens). In the editor that also auto-connects to the
   local trace store after engine init if nothing else did (`127.0.0.1`), and opening the window turns the channel on and
   starts a live session (`StartNetworkTrace`).
2. Open the window from the editor's Tools menu, entry "Network Prediction Insights" (nomad tab `NetworkPrediction`,
   registered in the Tools category). In standalone `UnrealInsights.exe` the tab opens by itself once a session with NP
   data is loaded.
3. Pick the PIE session in the toolbar (the `PieBegin` event separates them, so a dedicated-server + clients run of the
   harness shows up as several sessions), scrub the engine frame, select a simulation track, and read the frame content
   on the right. The search box filters on the text of the user states, so searching for a field name from your
   `ToString` finds the frames where it appears.

What you see depends on what your model traces: a state type with a precise `ToString` and per-field
`UE_NP_TRACE_RECONCILE` conditions is readable here; one without is a blank frame. The project types do this
(`FGSTN_CombatSyncCore::ShouldReconcile`, `FGSTN_InputSimState::ShouldReconcile`). The trace format is versioned on
the `NetworkPrediction` side (`ENetworkPredictionTraceVersion`, `Initial`); the analyzer stores the version and the tab
only auto-opens when it is non-zero, so a trace from a different fork of `NetworkPrediction` may not load.

## What this project changed vs. the stock engine plugin

Nothing. `git log --oneline -- Plugins/GASNPP/NetworkPredictionInsights` shows only the import commit `53c8716`, and
the sources carry no `GAS-NPP fork` or `Spike` markers. It is kept as part of the fork so the tracing of the forked
`NetworkPrediction` (whose own trace code is also unchanged by the project patches, see [PATCHES.md](../PATCHES.md))
has a matching viewer built from the same engine version. The import commit has no stock copy to diff against, so the
port itself cannot be audited from git alone.

## How to extend

There is little reason to. If you need to:

- New information in the timeline needs both ends: a new `UE_TRACE_LOG` event in `NetworkPrediction`'s
  `NetworkPredictionTrace.cpp` (a project patch, to be listed in PATCHES.md), a route in
  `FNetworkPredictionAnalyzer::OnAnalysisBegin` / `OnEvent`, storage in `FNetworkPredictionProvider` and a drawing
  change in `SNPSimFrameView` or `SNPSimFrameContents`. All of that is private C++ in this module.
- Another Insights tool that wants the parsed data reads it through `INetworkPredictionProvider`
  (`ReadNetworkPredictionProvider(Session)`), the only public API.
- For your own model you extend nothing here: write `ToString` and the `UE_NP_TRACE_RECONCILE` lines in your state
  types, as described in the [NetworkPrediction README](../NetworkPrediction/README.md#how-to-extend-writing-a-new-simulation-model).

No Blueprint surface; everything in the plugin is Slate and Trace Services code and stays C++.

## Using it standalone in another project

Copy `NetworkPredictionInsights` into the project's `Plugins/`; it needs no other project plugin and no ini. It is
useful only with a `NetworkPrediction` whose trace events it understands: this copy matches the fork in
`Plugins/GASNPP/NetworkPrediction` and Epic's 5.8 plugin (same trace code). To view traces in standalone
`UnrealInsights.exe` the program build must include the plugin (`SupportedPrograms` / `ProgramAllowList` already
allow it); the editor build gets it automatically.

## Where to read more

- [NetworkPrediction README](../NetworkPrediction/README.md) and Epic's [readme.txt](../NetworkPrediction/readme.txt)
  ("Getting Started" mentions `-trace=np` and the Tools menu entry).
- `Source/NetworkPrediction/Public/NetworkPredictionTrace.h` in the NetworkPrediction plugin: the macros a state type
  uses (`UE_NP_TRACE_RECONCILE`, `UE_NP_TRACE_SYSTEM_FAULT`...).
- [testing.md](../../../docs/architecture/testing.md): the PIE harness whose runs you would trace.
- [PATCHES.md](../PATCHES.md): the fork's change list (none for this plugin).
