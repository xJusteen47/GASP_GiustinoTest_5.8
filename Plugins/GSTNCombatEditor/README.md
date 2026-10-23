# GSTNCombatEditor

Editor tooling of GSTN Combat for Unreal Engine 5.8: the timeline editor (edit context, model, undo, the timeline panel, the
toolkit and track editors, then preview scene, gizmos), socket baking, the hurtbox set editor and validation. A designer builds a move with windows and keys instead
of typing frame numbers in a details panel. Everything the tooling writes is the plain data of `GSTNCombatCore`
(`FGSTN_CombatTimelineData`, the hurtbox data): the runtime never knows the editor exists, and assets stay editable in the
details panel.

Status: **experimental (0.4)**, `EnabledByDefault: false`, editor only. Checkpoints E1 (skeleton), E2 (edit context and
model), E4 (timeline panel) and E5 (toolkit, track editors) of [m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md); the design is
[combat-m5-editor.md](../../docs/architecture/combat-m5-editor.md). Disabling the plugin changes nothing in the game, the
harness or cooked data.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatEditor` | exists (Editor, PostEngineInit) | Module class (`FGSTN_CombatEditorModule`, log `LogGSTNCombatEditor`), `UGSTN_CombatEditorSettings`, the pure editing model of a timeline, the timeline panel with its commands, the track editor registry and the timeline asset editor (below). Covered by `GSTNCombat.Editor.*`. |

### The editing model (E2, no UI types)

| Type | File | Role |
|---|---|---|
| `FGSTN_TimelineEditContext` | `Public/Timeline/GSTN_TimelineEditContext.h` | What is edited: a `UGSTN_CombatTimeline` asset (complete) or the inline timeline of a Blueprint (declared, resolved in E13). Resolves the data on every call. |
| `FGSTN_TimelineEditContextRegistry`, `FGSTN_TimelineContextLease` | same | One editor per target: the second `Acquire` on the same asset is refused and the open editor is asked to focus. |
| `FGSTN_TimelineEditorModel` | `Private/Timeline/GSTN_TimelineEditorModel.h` | State (selection, playhead, view transform, tick rate, auto-key) and every mutation: add and remove track, move and resize window, total frames, first actionable frame, fit to montage, details edits, keys. The only writer of the data. Each mutation is one transaction, and does nothing, with no undo step, when it would change nothing. |
| `FGSTN_TimelineViewTransform` | same | Frame to pixel mapping shared by the future ruler, lanes and phase bar. |
| `FGSTN_TimelineDragTransaction` | `Private/Timeline/GSTN_TimelineUndo.h` | One drag, one undo step. The transaction opens at the first change; Escape restores the data (the editor's `CancelTransaction` only drops the record). |
| `FGSTN_TimelineUndoClient` | same | The `FEditorUndoClient` an owner (the toolkit) creates next to the model: after undo or redo it revalidates the selection and clamps the playhead. |
| `IGSTN_KeyedTrackAccess` and its registry | `Private/Timeline/GSTN_KeyedTrackAccess.h` | How the model reaches the keys of a keyed track (the hitbox track of M4) without knowing its struct. The key gestures (`AddKey`, `MoveKeys`, `DuplicateKeys`, `RemoveKeys`, copy and paste as text, `SetKeyTransform`) are written against it and tested with a test track; the hitbox track registers its adapter when it exists. |

Frames are integers. The gestures clamp to `[0, TotalFrames]` (a timeline of length 0 is clamped at 0 only). `ApplyTrackEdit` and
`ApplyDataEdit` (the details panel) write what they are given, because an invalid value is allowed there and the validator
reports it. Fitting `TotalFrames` to the montage is the explicit `FitTotalFramesToMontage`, never a side effect.

### The timeline panel (E4)

`SGSTN_TimelinePanel` is the frame timeline of combat-m5-editor 5.2: toolbar, ruler, phase bar, one row per track, frame data line.
It is a view of an `FGSTN_TimelineEditorModel` and nothing else: the widgets only call the model, each gesture is one
transaction (a drag goes through `FGSTN_TimelineDragTransaction`), and Escape restores. Everything that is not painting lives in
widget-free classes and runs in the automation tests without a window; the Slate files are the thin layer.

| Type | File (under `Private/Timeline/`) | Role |
|---|---|---|
| `FGSTN_TimelineLayout` | `GSTN_TimelineLayout.h` | Hit-testing a lane (keys before edges before body), box select, ruler tick and label spacing, milliseconds text, previous/next key or edge, selection span, wheel scroll and zoom, frame all/selection. `FGSTN_PlaybackClock`: whole frames from real time at the tick rate, looped. |
| `FGSTN_TimelineGesture` | `GSTN_TimelineGesture.h` | The mouse as a state machine over the model: move the selected windows (one shared clamp), resize an edge, move or Ctrl+duplicate keys, box select, the FAF marker, the tail start. |
| `FGSTN_TimelineActions` | same | Double-click (key or new track), Delete, K, copy, paste, duplicate and previous/next edge as model calls. |
| `FGSTN_PhaseBarData` | `GSTN_PhaseBarData.h` | Startup, active, recovery and actionable stretches, the tail and the summary line. **A stand-in for `FGSTN_TimelinePhases` of E3 (Core)**, deleted when E3 lands. |
| `FGSTN_TimelineEditorCommands` | `GSTN_TimelineEditorCommands.h` | The `TCommands<>` set, registered by the module. |
| `SGSTN_TimelinePanel`, `SGSTN_TimelineRuler`, `SGSTN_PhaseBar`, `SGSTN_TimelineTrackRow`, `SGSTN_TimelineLane`, `SGSTN_FrameDataSummary` | `Widgets/` | The widgets. Lanes, ruler and phase bar paint in `OnPaint` (no child widget per key). |

Frames are integers; a window is half open, `[StartFrame, EndFrame)`, drawn from the start of its first frame to the start of the
frame after its last. Drags clamp to `[0, TotalFrames]` through the model.

**Mouse.** Click selects (Shift adds or removes); drag the bar moves it, drag its ends resizes (cursor changes), drag a diamond
moves the key, Ctrl+drag a diamond copies it, drag in empty lane space box-selects (Shift adds), double-click empty lane space
adds a key (keyed tracks) or a window of the same kind. Wheel scrolls time, Ctrl+wheel zooms around the cursor. Click or drag
on the ruler moves the playhead. On the phase bar, drag the green FAF marker to write `FirstActionableFrame`, and the amber
handle of the hatched tail band to move the start of the cancellable tail. The wheel over a lane scrolls time, so the rows scroll
with their scrollbar or with the wheel over the name column.

**Commands** (Editor Preferences > Keyboard Shortcuts > GSTN Timeline Editor; every chord is rebindable):

| Command | Default | In E4 |
|---|---|---|
| Step Back / Forward | `,` / `.` | yes |
| Previous / Next Key | `Shift+,` / `Shift+.` | yes (keys and window edges) |
| Play | `Space` | yes, at the real tick rate, looped over the whole timeline |
| Add Key | `K` | yes, on the selected track that has keys |
| Auto-Key | `Shift+K` | toggles the model flag (used by the gizmos of E7) |
| Delete | `Delete` | yes: keys, else tracks |
| Copy / Paste / Duplicate Keys | `Ctrl+C` / `Ctrl+V` / `Ctrl+D` | yes (paste and duplicate at the playhead) |
| Frame All / Frame Selection | `Home` / `F` | yes |
| Fit Length to Montage | none | yes, the explicit command |
| Bake Sockets, Validate, Show Hitboxes/Hurtboxes/Movement Path | `Ctrl+B`, `F7`, none | declared, mapped by the toolkits of E6, E8, E10 |

### The timeline editor window (E5)

Double-click a `UGSTN_CombatTimeline` asset in the Content Browser (a Data Asset of that class, as before): the asset definition
`UGSTN_AssetDefinition_CombatTimeline` (`UAssetDefinitionDefault`) opens `FGSTN_TimelineEditorToolkit`, a standalone asset editor with
four tabs in the layout `GSTN_TimelineEditor_Layout_v1`: **Viewport** (a placeholder until the preview scene, E6), **Timeline** (the
panel of E4 under a warning bar), **Details** and **Frame Data**. Save is the editor's own Save; the layout is saved and restored by
the editor. A second toolkit on an open asset is refused and the open one comes to the front (the context registry of E2).

| Type | File | Role |
|---|---|---|
| `FGSTN_TimelineEditorToolkit` | `Private/Timeline/GSTN_TimelineEditorToolkit.h` | Owns the model, the undo client and the lease of the target; spawns the four tabs. |
| `UGSTN_TimelineSelectionProxy` | `Private/Timeline/GSTN_TimelineSelectionProxy.h` | A transient copy of the selected track, shown by a structure details view. `Commit` writes it back with `ApplyTrackEdit` (one undo step; a slider drag is one step through the model's drag). |
| `SGSTN_TimelineDetails` | `Private/Timeline/Widgets/` | The Details tab: follows the selection (one row at a time) and the data. |
| `SGSTN_MontageLengthBar` | same | Warning bar when the montage and `TotalFrames` disagree, with **Fit Total Frames to montage** (undoable, explicit, never automatic). |
| `SGSTN_TimelineToolbarExtra` | same | Loop range (editor state, not saved), skin and kit combos (stubs until E6), Validate (disabled until E10). |
| `IGSTN_TrackEditor`, `FGSTN_TrackEditorRegistry`, `IGSTN_GizmoProvider`, `FGSTN_TrackPreviewContext` | `Public/Timeline/GSTN_TimelineTrackEditor.h` | The extension point (below). |
| `GSTNTrackEditors` | `Private/Timeline/GSTN_TrackEditors.cpp` | The built-in track editors: CancelWindow, Movement, Tags, Event, WeaponSwitch. |

The names, colours, icons, bar labels and default lengths of the rows come from the track editors. A track struct nobody registered
(the Hitbox and HurtboxModifier shells until E5b, or a game's own track) gets the **generic row**: a window bar named by the struct,
its fields in the Details tab. Add Track lists the registered editors, then "Other track types" for the unregistered structs.

## Adding a track editor

A new runtime track (`FMyShieldTrack : FGSTN_TimelineTrack` in a runtime module) becomes a row, an Add Track entry and a Details
section by registering one `IGSTN_TrackEditor` from an **editor** module that depends on `GSTNCombatEditor`:

```cpp
#include "Timeline/GSTN_TimelineTrackEditor.h"

class FMyShieldTrackEditor final : public IGSTN_TrackEditor
{
public:
    virtual const UScriptStruct* GetTrackStruct() const override { return FMyShieldTrack::StaticStruct(); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("MyGame", "ShieldTrack", "Shield"); }
    virtual FName GetIconBrushName() const override { return TEXT("Icons.Shield"); }      // any FAppStyle brush name
    virtual int32 GetDefaultLength() const override { return 12; }                         // frames of a new window
    virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override             // text on the bar
    {
        return FText::AsNumber(static_cast<const FMyShieldTrack&>(Track).Strength);
    }
    virtual void InitNewTrack(FGSTN_TimelineTrack& Track, int32 AddedAtFrame, const FGSTN_CombatTimelineData&) const override
    {
        static_cast<FMyShieldTrack&>(Track).Strength = 100;                                // runs inside the add transaction
    }
};

void FMyEditorModule::StartupModule()  { FGSTN_TrackEditorRegistry::Get().Register(MakeShared<FMyShieldTrackEditor>()); }
void FMyEditorModule::ShutdownModule() { FGSTN_TrackEditorRegistry::Get().Unregister(FMyShieldTrack::StaticStruct()); }
```

Only `GetTrackStruct` and `GetDisplayName` are required. The colour defaults to the one the settings list for the struct
(`TrackColors`), so a designer re-colours it without code; override `GetColor` to fix one. `HasKeys` follows the keyed track access
registry. `DrawPreview` (E6) and `AsGizmoProvider` (E7) are declared and not called yet. A struct derived from a registered track keeps
its base's editor until it registers its own. Add a test: a registry case (listed, defaults applied by `AddTrack`) as in
`GSTN_EditorTests_Registry.cpp`.

Planned in this module, by checkpoint: hitbox and hurtbox-modifier track editors (E5b), preview scene (E6), gizmos (E7), baker (E8),
hurtbox set editor (E9), validators (E10, E11), details customizations (E12, E13).

## Dependencies

- Plugins: `GSTNCombat` (the timeline data), `GSTNCombatGAS` (kit data, and the `Combat.Hurtbox.*` tags the settings map to),
  `GSTNCombatDebug` as `Optional` (only to enable it together when it is present).
- `GSTNCombatEditor` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GSTNCombatCore; private ApplicationCore (clipboard),
  AssetDefinition (`UAssetDefinition`; `UAssetDefinitionDefault` itself is in UnrealEd), DeveloperSettings, EditorFramework, InputCore,
  PropertyEditor (the Details tab), Slate, SlateCore, UnrealEd (the toolkit), GSTNCombatGAS. The other editor modules are added by the
  checkpoints that need them.
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
(`IGSTN_TrackEditor`, "Adding a track editor" above); Editor Utility Widgets will call the Blueprint-callable `UGSTN_CombatEditorUtils` (tick rate,
validate, bake) added in E8 and E11.

## Replacing content

The plugin ships no content and references none by path. The preview kit, skin and mesh are soft references in the settings
(and, from E6, editor-only soft properties on the edited asset). A game picks its own in `DefaultGSTNCombatEditor.ini`.

## Tests

`Automation RunTests GSTNCombat.Editor`: `Module.LoadsAndRegisters`, `Settings.DefaultsFromPluginIni`,
`Settings.TickRateFromNetworkPrediction`, and `Model.*`: `EachMutationOnlyChangesWhatItSays`, `ClampsToTotalFrames`,
`UndoRedoRestoresExactData`, `DragIsOneUndoStep`, `CancelledDragLeavesNoStep`, `SelectionRevalidatedAfterUndo`,
`ClipboardRoundTrip`, `FitTotalFramesDoesNotRunSilently`, `TwoToolkitsOneTargetRefused`, `ViewTransformRoundTrip`. The model tests
run in the editor context (they need `GEditor->Trans`) on a transient timeline. `Timeline.*` (E4): `ViewTransformRoundTrip`,
`RulerTickSpacing`, `HitTestWindowEdges`, `HitTestTrackPriority`, `BoxSelect`, `NavigationAndPlayback`, `PhaseBarData`, the gestures
(`GestureMoveWindowIsOneUndoStep`, `GestureResizeEdges`, `GestureEscapeRestores`, `GestureGroupMoveAndSelection`,
`GestureKeysMoveAndDuplicate`, `GestureBoxSelect`, `GestureFirstActionableAndTail`), `Actions`, `CommandsRegistered`,
and `PanelConstructsAndRunsCommands` (skipped when the run has no Slate application). `Registry.*` (E5): `RegisteredTrackListed`,
`UnknownTrackGetsGenericRow`, `BuiltInsAllRegistered`, `DetailsProxyAppliesEditThroughModel`, `DetailsFollowsSelection` (the widgets
only with a Slate application), `AddTrackInitializesDefaults`, `TwoToolkitsOneAssetFocusesOpen` (the claim the toolkit makes, not the
window), `ToolkitLayoutAndAssetDefinition`, `PlayRangeIsEditorStateOnly`. Painting, mouse input and the toolkit window are not tested
headless: the smoke steps are in the commit messages of E4 and E5. The Hurtbox zone tags are covered by `GSTNCombat.GAS.Tags.HurtboxZonesRegistered`.
The canary build (plugin alone in a blank project, modularity §3.7 rule 7) is not run: the canary script does not exist yet.

## Changelog

- 0.4 (2026-10-23): timeline asset editor (toolkit, asset definition, Details tab through a selection proxy, montage-length bar, loop range), the track editor registry and the built-in track editors of the M3 kinds; the E4 test tab and its console command are removed (plan E5).
- 0.3 (2026-10-23): timeline panel (ruler, phase bar, rows, lanes, frame data), gestures, commands and the test tab (plan E4).
- 0.2 (2026-10-23): edit context, editor model, drag transaction, undo client, keyed track adapter (plan E2).
- 0.1 (2026-10-23): plugin and module skeleton, settings, tick rate helper (plan E1).
