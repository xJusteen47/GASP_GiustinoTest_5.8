# GSTNCombatEditor

Editor tooling of GSTN Combat for Unreal Engine 5.8: the timeline editor (edit context, model, undo, then toolkit, preview
scene, gizmos), socket baking, the hurtbox set editor and validation. A designer builds a move with windows and keys instead
of typing frame numbers in a details panel. Everything the tooling writes is the plain data of `GSTNCombatCore`
(`FGSTN_CombatTimelineData`, the hurtbox data): the runtime never knows the editor exists, and assets stay editable in the
details panel.

Status: **experimental (0.1)**, `EnabledByDefault: false`, editor only. Checkpoints E1 (skeleton) and E2 (edit context and
model) of [m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md); the design is
[combat-m5-editor.md](../../docs/architecture/combat-m5-editor.md). Disabling the plugin changes nothing in the game, the
harness or cooked data.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatEditor` | exists (Editor, PostEngineInit) | Module class (`FGSTN_CombatEditorModule`, log `LogGSTNCombatEditor`), `UGSTN_CombatEditorSettings`, and from E2 the pure editing model of a timeline. Covered by `GSTNCombat.Editor.*`. |

Planned in this module, by checkpoint: timeline panel (E4), toolkit, track editor registry (E5), preview scene (E6), gizmos
(E7), baker (E8), hurtbox set editor (E9), validators (E10, E11), details customizations (E12, E13).

## Dependencies

- Plugins: `GSTNCombat` (the timeline data), `GSTNCombatGAS` (kit data, and the `Combat.Hurtbox.*` tags the settings map to),
  `GSTNCombatDebug` as `Optional` (only to enable it together when it is present).
- `GSTNCombatEditor` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GSTNCombatCore; private DeveloperSettings,
  InputCore, UnrealEd, GSTNCombatGAS. Slate and the other editor modules are added by the checkpoints that need them.
- Not a dependency, on purpose:
  - `GSTNCombatNP` and `NetworkPrediction`: the one thing the editor needs from the fork, the fixed tick rate, is read from the
    Network Prediction settings object by reflection (`UGSTN_CombatEditorSettings::GetSimulationTickRate`), so the plugin links
    neither. The plan allows the `GSTNCombatNP` edge; it is added when a module really needs a type from it (the first
    candidates are the const accessors of E14/E15, which are the debug plugin's business).
  - `GSTNInput`: nothing reads input data yet (E10 validators will).
  - `GSTNCombatDebug`: the editor reaches it through `IGSTN_CombatDebugModule::Get()` at run time, never through `Build.cs`,
    because UnrealBuildTool stops when a named module does not exist, and the plugin must build with the debug plugin absent.
- No runtime module depends on this plugin (grep of every `*.Build.cs` and `.uplugin`, repeated at E19).

## Gameplay tags

This plugin declares **no** tag. It reads tags: the six `Combat.Hurtbox.{Head,Torso,Arm,Leg,Hand,Foot}` zone tags are native
and declared by `GSTNCombatGAS` (`GSTN_CombatTags.h`); the settings map bone names to them. Test tags, when a test needs
any, live in the project's test content ini, not here.

## Settings

Project Settings > Plugins > GSTN Combat Editor (`UGSTN_CombatEditorSettings`, `GSTNCombatEditor.ini`). Defaults ship in
`Config/DefaultGSTNCombatEditor.ini`; a project overrides them in its own `Config/DefaultGSTNCombatEditor.ini`.

| Setting | Default | Meaning |
|---|---|---|
| `DefaultPreviewKit`, `DefaultPreviewSkin` | none | Soft references the preview uses when an asset has none of its own (E6) |
| `bBakeOnSave` | off | Bake sockets when a timeline is saved (E8); baking is a button otherwise |
| `bAutoKeyDefault` | on | A gizmo drag on a frame without a key creates the key first (E7) |
| `TrackColors`, `GenericTrackColor` | one per M3 track kind | Row colours by track struct name |
| `FallbackTickRateHz` | 62.5 | Used only when the Network Prediction settings object cannot be read |
| `MirrorNamePairs`, `MirrorForwardAxis` | `_l/_r`, `Left/Right`, `.L/.R`; +X | Hurtbox mirroring (E9) |
| `HurtboxZoneRules` | bone fragment to `Combat.Hurtbox.*` | Zone suggested by the Physics Asset generator (E9) |
| `DebuggerKey` | NumPad 5 | Gameplay Debugger category key (E15) |

Lists use `+Name=` in an ini; to replace a default list, `!Name=ClearArray` first.

## Tick rate

`UGSTN_CombatEditorSettings::GetSimulationTickRate()` returns the Fixed tick rate the simulation runs at, 62.5 Hz here, with no
world (spike S2, [m5-spike-report.md](../../docs/notes/m5-spike-report.md)). Frame `f` of a timeline starts at `f / rate`
seconds.

## Extending in Blueprint

The editor is tooling, not simulation. A designer extends the **content** it edits (timelines, hurtbox sets, validation
severities and colours in the settings) without C++. New tools (a track editor, a gizmo provider) are a C++ extension point
(`IGSTN_TrackEditor` from E5); Editor Utility Widgets will call the Blueprint-callable `UGSTN_CombatEditorUtils` (tick rate,
validate, bake) added in E8 and E11.

## Replacing content

The plugin ships no content and references none by path. The preview kit, skin and mesh are soft references in the settings
(and, from E6, editor-only soft properties on the edited asset). A game picks its own in `DefaultGSTNCombatEditor.ini`.

## Tests

`Automation RunTests GSTNCombat.Editor`: `Module.LoadsAndRegisters`, `Settings.DefaultsFromPluginIni`,
`Settings.TickRateFromNetworkPrediction`. The Hurtbox zone tags are covered by `GSTNCombat.GAS.Tags.HurtboxZonesRegistered`.
The canary build (plugin alone in a blank project, modularity §3.7 rule 7) is not run: the canary script does not exist yet.

## Changelog

- 0.1 (2026-10-23): plugin and module skeleton, settings, tick rate helper (plan E1).
