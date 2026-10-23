# GSTNCombatDebug

Development tooling of GSTN Combat for Unreal Engine 5.8 that runs in PIE and development builds: the input display widget
(E14), later the Gameplay Debugger category (E15), debug draw (E16) and the combat state trace (E17). It reads combat state
through const accessors and a trace sink that the runtime modules expose; no runtime module depends on it.

Status: **experimental (0.2)**, `EnabledByDefault: false`. Checkpoints E1 (skeleton) and E14 (input display) of
[m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md); the design is
[combat-m5-editor.md](../../docs/architecture/combat-m5-editor.md) §10. Disabling it removes the tools and nothing else.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatDebug` | exists (Runtime, Default, not in Shipping) | `IGSTN_CombatDebugModule` (the interface other plugins may use; `CreateInputDisplay()` returns the input display widget), `LogGSTNCombatDebug`, the input display (below), its PIE overlay and, in an editor build, its editor tab. Covered by `GSTNCombat.Debug.*`. |

The descriptor excludes the module from Shipping with `TargetConfigurationDenyList: ["Shipping"]` (the spec's "bIsShippingExcluded"
rule: UnrealBuildTool has no such flag, the descriptor is where a module is kept out of a configuration).

## Input display (E14)

The input history of the **local player**, newest at the right, one column per simulated frame (`SGSTN_InputDisplay`).

| Row | What it shows |
|---|---|
| top | a marker on the frame a command came out; hover for the command tag |
| one per button (A..F, the six standard buttons) | press (bright), hold (dim), release (thin, orange), tap (cyan); a press a command used is **grey**, with its hold, and hover names the command |
| stick | the physical stick as numpad key and zone (`4t`, `8a`), blue by zone, a yellow bar on a flick (`!`) |
| under it | the direction relative to the character, where it differs |
| bottom | the events with the **frame gap** from the previous event in the notation of `FGSTN_InputScript`: `1:+A` is an attack press one frame after the previous event |

Where it shows:

- **PIE and development builds:** `gstn.Combat.Debug.InputDisplay 1` puts the strip in the lower right corner of every game or PIE viewport, fed by that viewport's local player, so a listen-server host and each client window show their own. `0` (default) removes it. Hit-test invisible.
- **Editor:** *Window > GSTN Combat > Input Display*, registered by this module itself (editor builds only). It follows the first PIE or game world that has a local player with a combat component, and has a box for a **scripted input**: type a notation such as `0:face=90 0:4t 6:6t! 7:+A`, press Enter, and the strip shows that history, greys the press the winning command used and the line under the box names the command (or says none matches). Empty box = live again. The slots it resolves against are the plugin's standard command set; handing it a kit's set is not done yet.

Where the data comes from: `UGSTN_CombatSimComponent::GetInputHistoryView()` (a const accessor, read outside the simulation) and the request the sync state holds (for the command markers, since the command event is published on the authority only). The widget never writes to the simulation.

The layout is a pure function of the history (`FGSTN_InputDisplayLayout::Build`, no Slate in its header): columns, cell kinds, consumed flags, markers, events and gaps; `ToNotation()` prints the strip as a script that replays to the same strip.

## Dependencies

- Plugins: `GSTNCombat`, `GSTNInput`, `GSTNInputNP`, `GSTNCombatNP` (the NP-of-same-layer exception of plan m5 R-B (c); it needs the GAS-NPP fork).
- `GSTNCombatDebug` (`Build.cs`): public Core, CoreUObject, Engine, SlateCore; private GameplayTags, Projects (tests read the plugin's sources), Slate, GSTNInputCore, GSTNInputNP (the command table), GSTNCombatCore, GSTNCombatNP (the component), and WorkspaceMenuStructure in editor builds. `GameplayDebugger` is added by E15.
- Nobody links this module, and the editor plugin has **no edge to it at all**: the editor tab belongs to this module, so with this plugin off the tab is just not there and `GSTNCombatEditor` builds and runs the same
  (`GSTNCombat.Editor.Debug.NoDependencyOnDebugModule`).

## Gameplay tags

None declared. Command names shown come from the command table's tags.

## Extending in Blueprint

The input display is a Slate widget (`SGSTN_InputDisplay`); the UMG wrapper for a designer's debug HUD is not done yet. The overlay and the tab need no Blueprint.

## Replacing content

The plugin ships no content and references none by path.

## Tests

`Automation RunTests GSTNCombat.Debug`: `Module.LoadsAndExposesInterface`, and `InputDisplay.*` (E14): `LayoutOfSampleHistory` (the notation sample: columns, gaps, facing key, flick, strip as a script), `ButtonRows`, `ConsumedPressGreyed`, `FlickMarker`, `ScriptedHistoryResolvesCommand`, `LayoutIsPure` (no Slate in the layout files), `NoLocalPawnShowsNothing`, `WidgetFollowsSourceAndScript` (skipped when the run has no Slate application). Painting, hover, the overlay in a viewport and the editor tab need a rendering editor and are checked by hand: open a PIE session (listen server, host + 2 clients), run `gstn.Combat.Debug.InputDisplay 1` in each window and press buttons while moving the stick; open *Window > GSTN Combat > Input Display* and type a notation.

## Changelog

- 0.2 (2026-10-23): input display: layout, widget, scripted history, PIE overlay (`gstn.Combat.Debug.InputDisplay`), editor tab registered by this module, `CreateInputDisplay()` implemented (plan E14).
- 0.1 (2026-10-23): plugin and module skeleton, module interface stub (plan E1).
