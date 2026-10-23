# GSTNCombatEditor

Editor tooling of GSTN Combat for Unreal Engine 5.8: the timeline editor (edit context, model, undo, the timeline panel, the
toolkit and track editors, then preview scene, gizmos), validation, socket baking and the hurtbox set editor. A designer builds a move with windows and keys instead
of typing frame numbers in a details panel. Everything the tooling writes is the plain data of `GSTNCombatCore`
(`FGSTN_CombatTimelineData`, the hurtbox data): the runtime never knows the editor exists, and assets stay editable in the
details panel.

Status: **experimental (0.7)**, `EnabledByDefault: false`, editor only. Checkpoints E1 (skeleton), E2 (edit context and
model), E4 (timeline panel), E5 (toolkit, track editors), E10 (validation) and E13 (inline mode) of [m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md)
(E3 is not built: the phase bar reads a private stand-in, `FGSTN_PhaseBarData`, until `GSTNCombatCore` gets `FGSTN_TimelinePhases`; E14, the input display, lives in `GSTNInputDebug` (plugin GSTNInput) with its combat adapter in `GSTNCombatDebug`); the design is
[combat-m5-editor.md](../../docs/architecture/combat-m5-editor.md). Disabling the plugin changes nothing in the game, the
harness or cooked data.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatEditor` | exists (Editor, PostEngineInit) | Module class (`FGSTN_CombatEditorModule`, log `LogGSTNCombatEditor`), `UGSTN_CombatEditorSettings`, the pure editing model of a timeline, the timeline panel with its commands, the track editor registry, the timeline asset editor, inline mode for ability Blueprints and the validators (below). Covered by `GSTNCombat.Editor.*`. |

### The editing model (E2, no UI types)

| Type | File | Role |
|---|---|---|
| `FGSTN_TimelineEditContext` | `Public/Timeline/GSTN_TimelineEditContext.h` | What is edited: a `UGSTN_CombatTimeline` asset, or the inline `Timeline` of an ability Blueprint (inline mode, below). Resolves the data on every call. |
| `FGSTN_TimelineEditContextRegistry`, `FGSTN_TimelineContextLease` | same | One editor per target: the second `Acquire` on the same asset is refused and the open editor is asked to focus. |
| `FGSTN_TimelineEditorModel` | `Private/Timeline/GSTN_TimelineEditorModel.h` | State (selection, playhead, view transform, tick rate, auto-key) and every mutation: add and remove track, move and resize window, total frames, first actionable frame, fit to montage, details edits, keys. The only writer of the data. Each mutation is one transaction, and does nothing, with no undo step, when it would change nothing. |
| `FGSTN_TimelineViewTransform` | same | Frame to pixel mapping shared by the ruler, lanes and phase bar. |
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
| Play | `Space` | yes, at the real tick rate, looped over the loop range of the toolbar (the whole timeline when none is set) |
| Add Key | `K` | yes, on the selected track that has keys |
| Auto-Key | `Shift+K` | toggles the model flag (used by the gizmos of E7) |
| Delete | `Delete` | yes: keys, else tracks |
| Copy / Paste / Duplicate Keys | `Ctrl+C` / `Ctrl+V` / `Ctrl+D` | yes (paste and duplicate at the playhead) |
| Frame All / Frame Selection | `Home` / `F` | yes |
| Fit Length to Montage | none | yes, the explicit command |
| Validate | `F7` | mapped by the toolkit (not the panel): the same as the toolbar button, see Validation |
| Bake Sockets, Show Hitboxes/Hurtboxes/Movement Path | `Ctrl+B`, none | declared, mapped by the toolkits of E6, E8 |

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
| `SGSTN_TimelineToolbarExtra` | same | Loop range (editor state, not saved), skin and kit combos (stubs until E6), Validate (runs the timeline rules, E10). |
| `IGSTN_TrackEditor`, `FGSTN_TrackEditorRegistry`, `IGSTN_GizmoProvider`, `FGSTN_TrackPreviewContext` | `Public/Timeline/GSTN_TimelineTrackEditor.h` | The extension point (below). |
| `GSTNTrackEditors` | `Private/Timeline/GSTN_TrackEditors.cpp` | The built-in track editors: CancelWindow, Movement, Tags, Event, WeaponSwitch. |

The names, colours, icons, bar labels and default lengths of the rows come from the track editors. A track struct nobody registered
(the Hitbox and HurtboxModifier shells until E5b, or a game's own track) gets the **generic row**: a window bar named by the struct,
its fields in the Details tab. Add Track lists the registered editors, then "Other track types" for the unregistered structs.

## Inline mode: the timeline of an ability Blueprint (E13)

A `UGSTN_GA_BaseCombat` Blueprint keeps a timeline of its own in the `Timeline` property of its class defaults (when `TimelineAsset` is
empty or not loaded; a loaded asset wins at run time). Timeline assets are the recommended home of a move, because several abilities and
weapons can share one; inline is supported for the one-off ability. The same editor edits both.

**Opening.** In the Blueprint editor, open Class Defaults: the `Timeline` row shows the frame data on one line (`Startup 7 | Active 3 |
Recovery 14 | FAF 24 | Total 30`) and an **Open Timeline Editor** button, over the same child rows as before, so the data is still editable in
the panel. The row (`FGSTN_TimelineDataCustomization`, registered for `FGSTN_CombatTimelineData`) is also on the `Data` of a timeline
asset. On any other `FGSTN_CombatTimelineData` (an instance, a struct in a Data Table, several objects selected) it shows the line and no
button. There is no custom Blueprint editor and no toolbar entry: the details row is the one door.

**What happens.** The window is the asset editor, with a strip above the timeline that names the Blueprint and says when a `TimelineAsset`
is also named (the inline timeline then does not play). Edits go through the same model, so every gesture is one transaction on the
**class default object** of the Blueprint's generated class, `Modify()` is called on it first, the owner gets `PostEditChangeProperty` for
`Timeline` and the Blueprint is marked modified (`FBlueprintEditorUtils::MarkBlueprintAsModified`: its package is dirty and its status
is out of date, exactly as after an edit in Class Defaults). Nothing recompiles the Blueprint; **Save** in the timeline window saves the
Blueprint (`GetObjectsCurrentlyBeingEdited` returns it).

| Piece | File | Role |
|---|---|---|
| `FGSTN_TimelineEditContext::ForBlueprint`, `FromOwnerObject`, `CanEditInline`, `IsInlineShadowedByAsset` | `Public/Timeline/GSTN_TimelineEditContext.h` | The inline half of the context. The Blueprint is the target (what is claimed and saved), the default object of its generated class the owner (what is recorded and undone). Both are looked up on every call; a Blueprint that is being compiled, was never compiled, or no longer derives from the ability does not resolve (`Get()` is null). `Modify()` also gives the owner `RF_Transactional`, which the default object a compile creates may not have. |
| `GSTNInlineTimeline` | `Private/Timeline/GSTN_InlineTimeline.h` | Plain functions: `OpenEditor`, `DecideAfterCompile` (refresh or close), `ExtractToAsset`, `GetExtractDefaults`. No Slate in the signatures, so the tests call them. |
| `FGSTN_TimelineDataCustomization` | `Private/Timeline/Details/` | The details row above. |
| `SGSTN_InlineTimelineBar` | `Private/Timeline/Widgets/` | The strip: the Blueprint's name, the note about `TimelineAsset`, **Extract to timeline asset...**. Collapsed for an asset. |

**Compile.** The toolkit listens to the Blueprint's `OnCompiled()`. A compile makes a new default object with the values carried over:
the model finds it by itself (nothing is cached across calls), the toolkit just revalidates the selection and redraws. When the
property is gone after a compile (the Blueprint was reparented to something that is not a combat ability), the toolkit shows a
notification and closes its window on the next tick. The compile callback itself is not the place to close: the Blueprint is still
being reinstanced around it.

**One editor per Blueprint.** The context registry refuses a second timeline editor on the same Blueprint and focuses the open one
(also across compiles: the target is the Blueprint, not its default object). The inline toolkit is registered with the asset editor
subsystem under the Blueprint, so closing or deleting the Blueprint closes it, but it says it is **not the primary editor** of the
Blueprint (`IsPrimaryEditor`), so double-clicking the Blueprint in the Content Browser still opens the Blueprint editor and does not
bring the timeline window to the front.

**Extract to timeline asset.** The button asks for a location (Content Browser save dialog, next to the Blueprint by default, named
`<Blueprint>_Timeline`), creates a `UGSTN_CombatTimeline`, copies the inline data into it, points the ability's `TimelineAsset` at it and
clears the inline `Timeline`, in **one** undo step on the default object, then closes the inline window and opens the asset in its own
editor. This is the spec's fallback for a Blueprint whose inline editing is not wanted (or is brittle), and the way to move to the
recommended workflow. An ability that already names a `TimelineAsset` plays that asset and ignores the inline data, so extracting would
switch what runs: the editor asks first (a Yes/No dialog before the save dialog) and does nothing on No. `GSTNInlineTimeline::ExtractToAsset`
refuses in that case unless it is given a confirmation callback that answers yes, which is how the tests drive it.

**Known limits.**
- A child Blueprint of an edited ability gets the new value through the normal compile of the parent: the model writes the parent's default
  object, it does not walk the derived classes the way the details panel does for a value it edits. Not verified in a rendering editor.
- Undo history from before a compile refers to the default object the compile replaced; undo is matched on the live one, so the first Ctrl+Z
  after a compile may find nothing to restore. The same holds for an edit made in Class Defaults.
- The inline editor edits `Timeline` even when `TimelineAsset` is set (the strip says it does not play); it never changes `TimelineAsset`
  except through Extract, which asks first when one is set.

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
registry. `Validate(Track, Data, Report)` adds the findings that are about your track kind (see Validation); the default adds none. `DrawPreview` (E6) and `AsGizmoProvider` (E7) are declared and not called yet. A struct derived from a registered track keeps
its base's editor until it registers its own. Add a test: a registry case (listed, defaults applied by `AddTrack`) as in
`GSTN_EditorTests_Registry.cpp`.

Planned in this module, by checkpoint: hitbox and hurtbox-modifier track editors (E5b), preview scene (E6), gizmos (E7), baker (E8),
hurtbox set editor (E9), the rest of the validation: M4 rules, the validation panel, Bake all (E11), details customizations (E12, E13).

## Validation (E10)

One set of rules, three readers: the **Data Validation** validators (on save, on demand, `-run=DataValidation` in CI), the **Validate**
button of the timeline editor (the findings go to the Message Log, category *GSTN Combat*), and from E11 the validation panel. The rules
are pure functions that fill an `FGSTN_ValidationReport`, so the automation tests call them with no editor window.

| Type | File | Role |
|---|---|---|
| `FGSTN_ValidationReport`, `FGSTN_ValidationFinding`, `EGSTN_Severity`, `FGSTN_FindingLocation` | `Public/Validation/GSTN_ValidationReport.h` | The result: severity (Error, Warning, Info), a stable `Code`, a message, an optional detail, the asset and where in it (track, key, frame). Includes no Slate. **Errors invalidate an asset, warnings do not.** |
| `FGSTN_CombatValidators` | `Private/Validation/GSTN_CombatValidators.h` | The rules and the checks of kit, weapon and loadout, as static functions. |
| `UGSTN_TimelineValidator`, `UGSTN_AbilityTimelineValidator`, `UGSTN_KitValidator`, `UGSTN_WeaponValidator`, `UGSTN_LoadoutPresetValidator` | `Private/Validation/GSTN_EditorValidators.h` | `UEditorValidatorBase` children: the Data Validation side. The ability validator takes a Blueprint whose generated class derives from `UGSTN_GA_BaseCombat` and runs the timeline rules on the `Timeline` of its default object (below). |
| `GSTNValidationLog` | `Private/Validation/GSTN_ValidationMessageLog.h` | A report as Message Log entries (an asset link, `[Code] message`, a **Show** action when a finding has a detail). |

Data Validation runs an asset's own `IsDataValid` first, then the validators. The assets already report what they check themselves (a
timeline its Core rules, a kit its own checks), so a validator does not say twice what the context already holds. What a validator adds
is what no asset could check alone: the track registry, the montage loaded at the project's tick rate, and the command table of a kit
with its weapons. An asset that is `Invalid` after its own check is not given to the validators.

**Timeline rules** (codes `Timeline.*`; `GSTN_TimelineValidation.h` of Core is wrapped, never repeated: its messages are classified into
the codes, and a test pins every one of its messages):

| Code | Source | Severity |
|---|---|---|
| `FrameRange` | Core (FirstActionableFrame, empty or late windows, the tail inside the timeline, events outside) plus `TotalFrames >= 1` and no negative frame | error; Core's warnings stay warnings |
| `MontageLength` | Core, with the montage loaded and the project's tick rate | warning |
| `DeferredCommit` | Core (nothing acts before the override window ends, the window ends inside the timeline) | error |
| `PartSelector`, `EmptyTrack`, `Ricochet` | Core, rules the spec table has no row for | as Core |
| `OverlapSameKind` | two overlapping `Movement` tracks; two `HurtboxModifier` tracks of different kinds on the same part, group or tag; a `Tags` window with the global lock next to one with a narrower lock | error, error, warning |
| `RootMotionMix` | a root motion montage and a `Movement` track whose mix mode is `Override` | warning |
| `CancelWindowTarget` | empty `IntoCommands`; no command of the kits that use the ability matches (only when the caller passes the commands); `OnHit`/`OnBlock` with no hitbox on the timeline | error, warning, warning |
| `TagsTrack` | a tag of a Tags window, an event cue, an event's gameplay event or the ricochet cue that is not in the tag table | error |
| `AimTurn` | `TurnUntilFrame` past the end (camera policy); `MaxTurnDegrees` not positive for a policy that turns | error |
| `UnknownTrack` | a track struct with no track editor (shown as a generic row) | warning |

The Hitbox same-group overlap, the baked-socket and hitbox rules arrive with E11 (they need M4).

**Inline timelines (E13).** `UGSTN_AbilityTimelineValidator` reads `Timeline` from the default object of the Blueprint's generated class and runs exactly `FGSTN_CombatValidators::ValidateTimeline` on it (`ValidateAbilityTimeline`; the findings belong to the Blueprint), so an inline timeline and an asset get the same codes for the same data, including `FrameRange` for a Blueprint that was never given a length. When the ability names a `TimelineAsset`, the asset plays and has its own validator: the inline data is not checked, and if it still holds something (a length, a track or a montage) the warning `Ability.InlineIgnored` says it is dead data. A `TimelineAsset` that does not resolve (the asset was deleted or renamed, or is not a combat timeline) is the error `Ability.TimelineAssetUnresolved`: the reference wins over the inline data, so the ability would play no timeline. A loaded asset resolves; otherwise the asset registry is asked (no load) and, when the validator may load, the asset is loaded to prove it. A class that was never compiled has no default object to read and is not validated.

**Kit, weapon and loadout.** Nothing new is decided here: the kit's and weapon's own `Validate` become findings (`Kit.Data`,
`Weapon.Data`: identity leaf, bindings, command set), the loadout rules of Core (`UGSTN_LoadoutRules`: allowed weapons, supers and
skins, customisations, the timeline registry) become `Loadout.Rule`, and the command table is compiled with the compiler the game
runs and checked with the input plugin's validators, once per weapon, like the game does. A slot conflict (`Slot.Ambiguous`,
`Slot.DuplicateCommand`, `Slot.InvalidSlot` errors, `Slot.Shadowed` warning) names **both slots** (their command tags), says which of them
comes from a weapon or a super, and carries a **Show** detail with each slot's input in the notation of M1 section 4, frames counted from
the slot's first event and the letters named (`A=Input.Button.Attack`). The weapon's check reports what the weapon adds to each kit that
can carry it (as its primary weapon or through the kit's weapon query), not the kit's own problems. Only slots that some binding uses are
in the table: a slot nobody binds can never fire, and is not compared.

There is no preset asset class in M3 (`UGSTN_LoadoutDefinition` is still to come), so the preset validator takes **any asset whose class
has an `FGSTN_LoadoutData` property**: that class when it arrives, or a game's own Blueprint holding one. It resolves the ids through the
Asset Manager and checks the loadout as the server will.

**Console and CI.** `UnrealEditor-Cmd <project> -run=DataValidation` runs every validator over the project's assets
(`-AssetType=GSTN_CombatTimeline` narrows it to a class); the exit code is 2 when an asset is invalid.

## Dependencies

- Plugins: `GSTNCombat` (the timeline data), `GSTNInput` (the slot validator and the notation), `GSTNCombatGAS` (kit, weapon and loadout
  data, and the `Combat.Hurtbox.*` tags the settings map to), `GSTNCombatNP` (since E13: the ability class `UGSTN_GA_BaseCombat`, see below) and the engine plugin `DataValidation`. Not `GSTNCombatDebug`: since E14 the debug plugin registers its own
  editor tab, so the two plugins have no edge in either direction.
- `GSTNCombatEditor` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GSTNCombatCore; private ApplicationCore (clipboard),
  AssetDefinition (`UAssetDefinition`; `UAssetDefinitionDefault` itself is in UnrealEd), DeveloperSettings, EditorFramework, InputCore,
  PropertyEditor (the Details tab and the timeline data row), Slate, SlateCore, UnrealEd (the toolkit, `FBlueprintEditorUtils`, `FKismetEditorUtilities`), ContentBrowser and AssetTools (the save dialog of Extract), GSTNCombatGAS, GSTNCombatNP, GSTNInputCore, and for validation DataValidation
  (`UEditorValidatorBase`), MessageLog, AssetRegistry, Projects, GameplayAbilities (the ability base class `UGSTN_GA_BaseCombat`, and the test bindings, derive from `UGameplayAbility`). The module
  is compiled file by file (`bUseUnity = false`): the tests share helper names through file-scope using-directives. The other editor
  modules are added by the checkpoints that need them.
- The `GSTNCombatNP` edge (plan m5 R-B exception (c), the editor to NP edge, added by E13): inline mode needs the type of the ability whose `Timeline`
  and `TimelineAsset` a Blueprint edits, `UGSTN_GA_BaseCombat`, for the cast on the default object, the class check of the context and the
  validator. It is the only NP type the editor names. It brings Network Prediction, Mover and the ability system simulation into the editor's
  link graph (they are loaded by the project anyway); the editor still touches none of their APIs, and the fixed tick rate is still read from
  the Network Prediction settings object by reflection.
- Not a dependency, on purpose:
  - `NetworkPrediction` itself, see above.
  - `GSTNCombatDebug`: no module, plugin or include edge. The *Window > GSTN Input > Input Display* tab belongs to the input debug module (`GSTNInputDebug`, plugin GSTNInput),
    which registers it in an editor build; with that plugin off the tab does not exist and this plugin builds and runs the same.
    `GSTNCombat.Editor.Debug.NoDependencyOnDebugModule` keeps it so.
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

`Automation RunTests GSTNCombat.Editor`: `UiFixes.*` (UI test fixes: `PlayheadWorksWithoutLength`, `TotalFramesAndFafAreUndoable`, `EmptySpaceClickClearsSelection`, `TextIsReadableOnEveryBar`, `FindingsStoreAndList`, `GoToFindingSelectsAndMoves`, `LengthFieldsConstruct`), `Module.LoadsAndRegisters`, `Debug.NoDependencyOnDebugModule` (E14: no debug module, plugin or include in this plugin), `Settings.DefaultsFromPluginIni`,
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
window), `ToolkitLayoutAndAssetDefinition`, `PlayRangeIsEditorStateOnly`. `Validate.*` (E10): one failing and one passing case for each timeline rule (`FrameRange`, `MontageLength`, `OverlapSameKind`, `RootMotionMix`, `DeferredCommit`, `CancelWindowTarget`, `TagsTrack`, `UnregisteredTagIsError`, `AimTurn`, `UnknownTrack`), `CleanTimelineHasNoFindings`, `CoreMessagesClassified`, `TrackEditorValidate`, `KitConflictFlagsBothSlotNames`, `FixClearsTheError`, `WeaponReportsWhatItAdds`, `KitCanCarryWeapon`, `PresetConflictNamesWeaponOrSuper`, `PresetValidatorTakesAnyLoadoutHolder`, `SlotInputNotation`, `ErrorsInvalidateWarningsDoNot`, `SkipsWhatIsDataValidSaid`, `FindingMessage`, `ReportIsWidgetFree`. `Inline.*` (E13), on a real Blueprint of `UGSTN_GA_BaseCombat` made and compiled in a temporary package: `ContextResolvesCdoTimeline`, `SurvivesBlueprintCompile`, `ToolkitClosesWhenPropertyGone`, `EditMarksBlueprintModified`, `UndoRestoresCdoValue`, `ExtractCreatesAssetAndClearsInline`, `TwoToolkitsSameBlueprintRefused`, `BarWording`, and `Validate.AbilityInlineTimeline`. `Model.WeaponSwitchIsDrawnAndMovedOnSwapFrame`, `Validate.WeaponSwitchFrameMismatch` and `Inline.ExtractNeverReplacesANamedAssetWithoutConfirmation` (review fixes). Painting, mouse input, the toolkit window, the details row and the save dialog of Extract are not tested
headless; check them by hand in a rendering editor: open a timeline asset and an ability Blueprint's inline timeline, add, drag and resize tracks and keys, undo and redo, play with Space, Validate (F7) and Extract. The Hurtbox zone tags are covered by `GSTNCombat.GAS.Tags.HurtboxZonesRegistered`.
The canary build (plugin alone in a blank project, modularity §3.7 rule 7) is not run: the canary script does not exist yet.

### Reading the timeline (UI test fixes, 0.7.3)

- **Playhead with no length.** A new inline timeline has `TotalFrames` 0. The playhead, the loop range, Play and Frame All now work over
  `FGSTN_TimelineEditorModel::GetPlayableFrames`: `TotalFrames`, or while it is 0 the end of the content (tracks and FAF) and never less than
  `MinimumRulerFrames` (30, the span Frame All already showed). Ruler clicks, Step Forward and Play therefore move the playhead on a timeline that has no length.
- **Length and FAF fields.** `SGSTN_TimelineLengthFields` ("Length [n] frames  FAF [n]") sits under the lanes of the timeline panel and above the summary of
  the Frame Data tab. Typing is one undo step (`Set timeline length`, `Move first actionable frame`); dragging a field is one step for the drag.
- **Colours.** Bars are a dark fill (`GSTNTimelineColors::BarFill`, 0.4 of the track colour) inside a 1 px outline of the track colour; the text on a bar, on a phase and on the
  name cell of a row is white or black by the luminance of its background (`ReadableTextOn`, WCAG contrast at least 4.5:1), never a fixed colour. A selected track row has a dark blue name
  cell (`SelectedHeaderFill`); a selected bar gets a white outline. Startup is blue, active red, recovery orange, actionable green, and a phase segment wide enough carries its name and length.
  Backgrounds and text come from the editor theme (`FStyleColors`), not literal values. All of it is in `Private/Timeline/GSTN_TimelineColors.h`, with no Slate in it.
- **Findings in the Frame Data tab.** `FGSTN_FindingsStore` keeps the report of the last Validate (the same one the Message Log gets, one run); `SGSTN_FindingsList` lists it with severity icons and the
  `[Code] message` text, goes stale (and says so) when the data changes afterwards, and a double click selects the finding's track and puts the playhead on its frame (`FGSTN_TimelineActions::GoToFinding`).
- **Empty space clears the selection.** A click that no row takes (under the last track) falls through the list to a border that calls `FGSTN_TimelineActions::ClickEmptySpace` (Shift keeps the selection).
  The selection is editor state: no transaction, the undo history is untouched.

## Changelog

- 0.7.3 (2026-10-23, UI test fixes): the playhead works while `TotalFrames` is 0, `Length` and `FAF` fields (timeline panel and Frame Data tab), readable bar, phase and header colours with distinct phase colours, the findings of Validate listed in the Frame Data tab, a click on empty space clears the selection; test file `GSTN_EditorTests_UiFixes.cpp` (`GSTNCombat.Editor.UiFixes.*`).
- 0.7.2 (2026-10-23, review fixes): an unresolvable `TimelineAsset` on an ability is the error `Ability.TimelineAssetUnresolved`; a `SwapFrame` typed in the details panel moves the weapon switch window's `StartFrame` with it; a header comment about the weapon switch frame.
- 0.7.1 (2026-10-23, review fixes): the weapon switch is drawn, hit-tested, moved and clamped on `SwapFrame` like the runtime and the validation (moving it also repairs a stale `StartFrame`), with a `WeaponSwitch.FrameMismatch` warning; Extract no longer replaces a named `TimelineAsset` without a confirmation, and drops the new asset when it is refused.
- 0.7 (2026-10-23): the descriptor no longer lists `GSTNCombatDebug` as an optional plugin and a test keeps the editor free of any edge to it; the input display tab is registered by the debug plugin (plan E14).
- 0.6 (2026-10-23): inline mode: the timeline of an ability Blueprint edited in the same window through the class default object, the Open Timeline Editor row, the compile handling, Extract to timeline asset, the ability timeline validator, and the `GSTNCombatNP` edge for the ability class (plan E13).
- 0.5 (2026-10-23): validation: the report and the rules of the M3 timeline, the Data Validation validators of timeline, kit, weapon and loadout preset, the slot conflict findings with both slot names and the M1 notation, the Message Log category, the Validate button and F7, the `Validate` hook of the track editor (plan E10).
- 0.4 (2026-10-23): timeline asset editor (toolkit, asset definition, Details tab through a selection proxy, montage-length bar, loop range), the track editor registry and the built-in track editors of the M3 kinds; the E4 test tab and its console command are removed (plan E5).
- 0.3 (2026-10-23): timeline panel (ruler, phase bar, rows, lanes, frame data), gestures, commands and the test tab (plan E4).
- 0.2 (2026-10-23): edit context, editor model, drag transaction, undo client, keyed track adapter (plan E2).
- 0.1 (2026-10-23): plugin and module skeleton, settings, tick rate helper (plan E1).
