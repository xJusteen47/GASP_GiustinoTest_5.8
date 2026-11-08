# GSTNCombatDebug

Development tooling of GSTN Combat for Unreal Engine 5.8 that runs in PIE and development builds: the combat side of the input display
and the Gameplay Debugger category (E15) and the BakeCheck overlay (E16) now, the combat state trace (E17) later. It reads combat state through const
accessors and a trace sink that the runtime modules expose; no runtime module depends on it.

Status: **experimental (0.4)**, `EnabledByDefault: false`. Checkpoints E1 (skeleton), E14 (input display), E15 (Gameplay Debugger category) and E16 (BakeCheck) of
[m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md); the design is
[combat-m5-editor.md](../../docs/architecture/combat-m5-editor.md) §10. Disabling it removes the tools and nothing else.

**The input display moved.** The strip, the corner overlay, the editor tab, the scripted box and the layout are an *input* tool and live in
the plugin [GSTNInput](../GSTNInput/README.md#input-display-gstninputdebug) (module `GSTNInputDebug`), which reads an input history through
`IGSTN_InputHistorySource`. What stays here is what only the combat plugin knows.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatDebug` | exists (Runtime, Default, not in Shipping) | `IGSTN_CombatDebugModule` (the exported interface other plugins may use: `Get()`, `IsAvailable()`, and `CreateInputDisplay()`, which forwards to the input plugin's display), `LogGSTNCombatDebug`, and the **combat adapter** of the input display (below). Covered by `GSTNCombat.Debug.*`. |

The descriptor excludes the module from Shipping with `TargetConfigurationDenyList: ["Shipping"]` (the spec's "bIsShippingExcluded"
rule: UnrealBuildTool has no such flag, the descriptor is where a module is kept out of a configuration). The **Test** configuration
and the Development client and server builds still include it; add `"Test"` to the deny list if a Test build must not carry it.

Other plugins reach it through `IGSTN_CombatDebugModule::Get()` (null when the plugin is disabled or excluded), which needs this
module in their dependencies because the interface is exported; a plugin that must not depend on it can call
`FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>("GSTNCombatDebug")` itself and needs only the header's include path.

## The combat adapter of the input display

At startup the module registers three things in `FGSTN_InputSourceRegistry` of `GSTNInputDebug` and removes them at shutdown:

| What | Role |
|---|---|
| a **finder** (`FGSTN_CombatInputSource::Find`) | for a pawn with a `UGSTN_CombatSimComponent`: a source that reads `GetInputHistoryView()` (the const accessor, outside the simulation) and the request the sync state holds. The command markers come from that request, because `Input.Event.CommandExecuted` is published on the authority only and a predicting client would never see its own commands. |
| the **default profile** | the six standard combat buttons in the order of the notation (A attack, B strong, C special, D crouch, E shield, F jump) and the plugin's standard command set (`UGSTN_StandardSlots::Build()`), so the scripted box answers "what does this input do" before any play session. Handing it a kit's composed set is not done yet. |
| the console variable `gstn.Combat.Debug.InputDisplay` | the old name of `gstn.Input.Debug.InputDisplay`: setting it sets the new one, so scripts and habits keep working. |

The display never writes to the simulation: the source copies the history and the request, nothing else. See the input plugin's README for the
strip, the focus following and the picker.

## Gameplay Debugger category (E15)

Open the Gameplay Debugger in PIE (apostrophe key), select a pawn, and the category **GSTNCombat** (slot 6, on by default) shows:

- frame, combat clock and action state; the running timeline with its frame (hitstop frames excluded) and source; hitstop and hitstun frames left;
  active weapon, loadout and table revision; a red line when the combat gate ran closed (a missing table set);
- the input history in notation, held buttons and the last request (`GetDebugString`);
- the last 8 hits dealt (white) and received (orange): frame, the other pawn, damage, the event's tags;
- in the world: a cyan facing arrow and, above the head, `T<timeline> f<frame>` or the action state.

It collects on the authority (the listen host or the dedicated server) and the debugger replicates the lines, so a client sees the
authority's view of the pawn. The hit ring is `UGSTN_CombatDebugHitRecorder` (world subsystem), fed by the `Combat.Event.HitLanded` and
`Combat.Event.HitReceived` outbox events of `UGSTN_SimEventSubsystem` (published once, on the authority): it never reads the sync state.
`gstn.Combat.Debug.HitRing 0` stops recording. The other spec items of 10.2 not shown yet: the buffered command and why the last one resolved,
the windows open now (cancel, override, hurt state) and the lock tags.

## BakeCheck (E16)

`gstn.Combat.Debug.BakeCheck 1` draws, for every pawn that runs a timeline with baked sockets on this machine (authority or owning
client, never a simulated proxy), each baked socket placed as the attacker step places it (baked root-relative transform * the pawn's
transform, cyan) next to the live mesh socket of the same name (magenta), a line and the gap in cm and degrees. `2` also logs one
`BakeCheck <pawn> T<n> f<frame> <socket> live=<0|1> d=<cm> a=<deg>` line per socket and frame. Off by default. The hitbox and hurtbox
draw already existed in `GSTNCombatNP` (`gstn.Combat.Debug.Hitboxes`, `.Hurtboxes`, drawn from the snapshot on the simulating machine) and
were not moved.

## Trace (E17)

`gstn.Combat.Debug.Trace 1` (default 0; set it in the console or on the command line, it installs or removes the sink when it changes) makes the debug
module the sink of the combat trace: every simulated frame of every combat entity writes a `CombatFrame` event (entity, frame, re-simulated flag,
action state, timeline index and frame, hitstop and hitstun frames still to come, input history head) and the combat events write `CombatEvent`
(command executed, hit landed, adoption, cancel, rollback with the re-simulated frame range, plus a bookmark for rollbacks) on the Unreal Insights
channel `GSTNCombat`. Start the session with `-trace=default,GSTNCombat` (or `Trace.Enable GSTNCombat` in the console) and open the trace in Insights.
With the variable at 0 no sink is installed and the simulation pays one pointer load and a branch per entity per frame. The call sites are in
`GSTNCombatNP` and compile out of Shipping.

## Dependencies

- Plugins: `GSTNCombat`, `GSTNCombatGAS` (the hit event tags), `GSTNInput` (the display lives in its `GSTNInputDebug` module), `GSTNInputNP`, `GSTNCombatNP` (the component; it needs the GAS-NPP fork).
- `GSTNCombatDebug` (`Build.cs`): public Core, CoreUObject, Engine, SlateCore; private GameplayTags, Projects (tests read the plugin's sources), Slate, GSTNInputCore, GSTNInputNP, GSTNInputDebug (the registry), GSTNCombatCore, GSTNCombatGAS, GSTNCombatNP, and `GameplayDebugger` through `SetupGameplayDebuggerSupport`.
- Nobody links this module yet, and the editor plugin has **no edge to it at all**: with this plugin off the editor builds and runs the same
  (`GSTNCombat.Editor.Debug.NoDependencyOnDebugModule`).

## Gameplay tags

None declared.

## Extending in Blueprint

The input display is a Slate widget; the UMG wrapper for a designer's debug HUD is not done yet. Nothing here needs Blueprint.

## Replacing content

The plugin ships no content and references none by path.

## Tests

`Automation RunTests GSTNCombat.Debug`: `Module.LoadsAndExposesInterface`, `Module.InterfaceIsExported`, and `InputDisplay.*`: `ConsumedPressGreyed` and `ScriptedHistoryResolvesCommand` (the scripted input resolved against the standard command set, through the
input plugin's `FGSTN_InputDisplayScript`), `CombatSourceIsRegisteredWithTheInputDisplay` (the finder, the default profile, the forwarding of `CreateInputDisplay`) and `OldConsoleVariableSetsTheNewOne`; `GameplayDebugger.HitRingKeepsLastEightNewestFirst`, `.DescribeCoreLines`, `.RecorderFilesHitsBySide`,
`.CategoryWithoutCombatComponent`; `BakeCheck.ComparePlacesBakedSocketLikeTheAttacker`, `.ConsoleVariableOffByDefault`. The layout, the widget, the metrics and the
player selection are tested in `GSTNInput.Debug.*`. Painting, hover, the overlay in a viewport and the editor tab need a rendering editor: open a PIE session (listen server, host + 2 clients), run `gstn.Input.Debug.InputDisplay 1` in each window and press
buttons while moving the stick; open *Window > GSTN Input > Input Display*, click each window and type a notation.

## Changelog

- 0.5 (E17): combat trace sink and `gstn.Combat.Debug.Trace`; tests `GSTNCombat.Debug.Trace.*` (5).
- 0.4 (2026-11-06, E15, E16): Gameplay Debugger category `GSTNCombat` with the hit ring recorder; `gstn.Combat.Debug.BakeCheck`; tests
  `GSTNCombat.Debug.GameplayDebugger.*` (4) and `GSTNCombat.Debug.BakeCheck.*` (2).
- 0.3 (2026-10-23, input display rework): the display, overlay, tab, layout and script player moved to `GSTNInputDebug` (plugin GSTNInput); this module keeps the combat adapter (finder, default profile) and the old console variable name as an alias.
- 0.2.1 (2026-10-23, review fixes): `IGSTN_CombatDebugModule` is exported (`GSTNCOMBATDEBUG_API`) so `Get()` and `IsAvailable()` link from other modules; test `Module.InterfaceIsExported`.
- 0.2 (2026-10-23): input display: layout, widget, scripted history, PIE overlay, editor tab registered by this module, `CreateInputDisplay()` implemented (plan E14).
- 0.1 (2026-10-23): plugin and module skeleton, module interface stub (plan E1).
