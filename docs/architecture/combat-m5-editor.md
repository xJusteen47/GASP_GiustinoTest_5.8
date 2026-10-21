# Combat M5 — Editor tooling (`GSTNCombatEditor`) (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §3, §11, §12.1 and §14 there, and the M1–M4 specifications
(M3 §6.3 and M4 §3 define the data this editor edits). Section numbers below are local to this file. Expands the M5
outline of [combat-m5-m9.md](combat-m5-m9.md).

> **Status:** written before M3/M4 are implemented. Places that depend on their outcome are marked **(M3/M4 result)**;
> re-read the finished headers before starting and fix this file where they differ. Items marked *Proposal* resolve an
> open question of the outline; the owner decides later.

## 1. Scope

**Goal:** a designer builds a three-hit jab string with a baked-socket hitbox, cancel windows and an EX version, and
sees frame data, hurtboxes and conflicts, **without typing a frame number in a details panel**. Everything the editor
writes is the plain data of M3/M4 (`FGSTN_CombatTimelineData`, `UGSTN_HurtboxSetDefinition`): the runtime never knows
the editor exists, and assets stay editable in the details panel as before.

In:

- Timeline editor (asset toolkit for `UGSTN_CombatTimeline`, same widgets for the inline timeline of a
  `UGSTN_GA_BaseCombat` Blueprint): preview scene, frame timeline, gizmos, details, derived frame data.
- Socket baking at the NPP tick rate, with staleness detection.
- Hurtbox set editor (generate from the Physics Asset, mirror, gizmos).
- Validation: `IsDataValid` for timelines, hurtbox sets, kits, weapons, loadout presets, plus one validation panel.
- Debug: input display widget, Gameplay Debugger category, trace of the combat state, debug draw (`gstn.Combat.Debug.*`).

Out: Sequencer integration (decided against, §2.1); editing kits/weapons/bindings visually (they stay details-panel
assets; only their validation gets UI); runtime features of M6+ (their tracks plug in through §12); a visual graph for
conditions or patterns; cooking/packaging of editor data (nothing editor-only is cooked, §7.3).

## 2. Decisions and spikes first

### 2.1 Resolved open items (*Proposal*, in the order of the outline)

1. **Custom Slate timeline, not Sequencer** (outline recommendation confirmed). The data is frame-indexed structs in an
   array of `TInstancedStruct`, not keyframed properties on bindings; a `UMovieScene` mirror would need two-way sync
   and its own undo. Cost accepted: ruler, zoom, selection and snapping are written here (about 1500 lines of Slate).
   Widgets are written against `FGSTN_TimelineEditorModel` (§4.2) so a Sequencer-style skin could replace them later.
2. **Preview without Persona**: `FAdvancedPreviewScene` + a `UDebugSkelMeshComponent` + our own `FEditorViewportClient`.
   Persona's toolkit brings skeleton trees, animation tabs and asset-specific assumptions we would fight. Revisit only
   if the montage preview (§5.4) cannot be reproduced without Persona's anim instance setup.
3. **Inline timelines in a Blueprint**: no custom Blueprint editor. A `IPropertyTypeCustomization` for
   `FGSTN_CombatTimelineData` shows an **Open Timeline Editor** button and a frame-data summary line; it opens the same
   toolkit in *inline mode*, bound to the Blueprint (§4.1). Edits go to the class default object and mark the
   Blueprint modified, so it behaves like editing the property in Class Defaults.
4. **Debug traces**: v1 = Gameplay Debugger category + structured trace events (`UE_TRACE_LOG`) + bookmarks in
   Unreal Insights; a **Rewind Debugger track** is a stretch step (M5.8) gated on spike S3. Reason: a Rewind Debugger
   track needs a TraceServices analyzer and a track creator, a sizeable piece with an API that moves between releases.
5. **Where the debugger lives**: the Gameplay Debugger category, the input display widget and the debug draw run in
   PIE/dedicated/development builds, so they cannot sit in an Editor module. New runtime module **`GSTNCombatDebug`**
   (`Runtime`, excluded from Shipping, §3) instead of overloading `GSTNCombatNP`.
   `GSTNCombatEditor` keeps only editor-only code (asset editors, baking, validation UI, menus).
6. **Montage mismatch** (montage length ≠ `TotalFrames`): the editor never rewrites `TotalFrames` silently; it shows a
   warning bar with **Fit Total Frames to montage** (undoable) and keeps the validator warning of M3.
7. **Stale bakes** block cooking (error in `IsDataValid`) but not editing; baking is explicit (button, plus an opt-in
   *Bake on save* in editor preferences, default off).

### 2.2 Spikes (half a day each; record results in combat.md §17)

- **S1 Montage sampling**: in a throwaway editor function, evaluate a montage section on a `UDebugSkelMeshComponent` at
  frames 0..N (position set directly, no real-time tick) and read a socket's component-space transform. Check:
  repeatable to 1e-4 cm across runs, equal when sampled backwards, root motion handling (does the evaluated pose
  already exclude extracted root motion?), notifies/blend-in not influencing the pose, additive layers, anim
  instances with a post-process (GASP). The bake algorithm (§7) depends on the result. **(M3 result)**: how
  `PlaySyncedMontage` starts the montage (blend-in time, play rate, section) must match what the bake assumes.
- **S2 Tick rate source**: confirm in the fork which setting holds the fixed tick rate (`UNetworkPredictionSettings`,
  fixed tick frame rate / `FixedTickFPS`, 62.5 Hz here) and that the editor can read it without a world. If it can
  only be read from a world manager, fall back to the project-settings default object.
- **S3 Insights/Rewind**: with the fork, does Rewind Debugger record anything for NP-simulated pawns? Is there an NP
  Insights provider ("NP Insights" in combat.md §14)? Decide M5.8 scope from the answer.
- **S4 Gizmo in a custom viewport**: `FEditorViewportClient::GetWidgetLocation` / `InputWidgetDelta` on a virtual
  selection (a hitbox key is not a component). Prototype with one box and verify undo.

## 3. Modules

Two new modules, each in its **own plugin** (section 3b). Plugin and module are named alike.

| Module | Type | Public dependencies | Private dependencies |
|---|---|---|---|
| `GSTNCombatEditor` | `Editor`, `LoadingPhase = PostEngineInit` | Core, CoreUObject, Engine | Slate, SlateCore, InputCore, UnrealEd, EditorFramework, EditorSubsystem, PropertyEditor, EditorWidgets, AdvancedPreviewScene, ToolMenus, ToolWidgets, ContentBrowser, AssetTools, Kismet, MessageLog, DataValidation, GameplayTags, GameplayTagsEditor, StructUtils, StructUtilsEditor (names as of 5.8: verify), PhysicsCore, AnimGraphRuntime, `GSTNCombatCore`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNInputCore`, NetworkPrediction; **dynamic** (no link): `GSTNCombatDebug` |
| `GSTNCombatDebug` | `Runtime`, compiled only when `!Target.bBuildEditor ? Target.Configuration != Shipping : true` (`bIsShippingExcluded`) | Core, CoreUObject, Engine, Slate, SlateCore | GameplayDebugger (guarded by `WITH_GAMEPLAY_DEBUGGER`), TraceLog, GameplayTags, `GSTNCombatCore`, `GSTNCombatNP`, `GSTNInputCore` |

Rules: `GSTNCombatCore`/`GSTNCombatGAS`/`GSTNInput*` never depend on either new module (checked by acceptance 8).
`GSTNCombatNP` stays free of Slate: it only exposes read-only accessors (§10.2) that the debug module uses. The editor
module reaches the debug module for the shared widgets (`SGSTN_InputDisplay`) and trace schema.

```
Plugins/GSTNCombatEditor/Source/GSTNCombatEditor/
  Public/
    GSTNCombatEditorModule.h                 FGSTN_CombatEditorModule (IModuleInterface); registers everything in 3.1
    Timeline/GSTN_TimelineTrackEditor.h      IGSTN_TrackEditor, FGSTN_TrackEditorRegistry (extension point, section 12)
    Timeline/GSTN_TimelineEditContext.h      FGSTN_TimelineEditContext (asset or inline target, section 4)
    Bake/GSTN_SocketBaker.h                  UGSTN_SocketBakerLibrary / FGSTN_SocketBaker (section 7)
    Validation/GSTN_ValidationReport.h       FGSTN_ValidationFinding, FGSTN_ValidationReport (section 8)
  Private/
    Settings/GSTN_CombatEditorSettings.h/.cpp        UGSTN_CombatEditorSettings (UDeveloperSettings, editor preferences)
    Assets/GSTN_AssetDefinition_CombatTimeline.*     UAssetDefinitionDefault: colour, open action, thumbnails
    Timeline/GSTN_TimelineEditorToolkit.*            FGSTN_TimelineEditorToolkit : FAssetEditorToolkit, FEditorUndoClient
    Timeline/GSTN_TimelineEditorModel.*              FGSTN_TimelineEditorModel (state and mutations, no Slate)
    Timeline/GSTN_TrackEditors.cpp                   built-in IGSTN_TrackEditor implementations
    Timeline/Widgets/SGSTN_Timeline{Panel,Ruler,TrackRow,Lane}.*, SGSTN_PhaseBar.*, SGSTN_FrameDataSummary.*
    Timeline/Details/GSTN_HitDataCustomization.*     IPropertyTypeCustomization for FGSTN_HitData
    Timeline/Details/GSTN_TimelineDataCustomization.*  FGSTN_CombatTimelineData: "Open Timeline Editor" + summary
    Preview/GSTN_TimelinePreviewScene.*              FGSTN_TimelinePreviewScene : FAdvancedPreviewScene
    Preview/GSTN_TimelineViewportClient.*            FGSTN_TimelineViewportClient : FEditorViewportClient
    Preview/GSTN_PreviewDrawing.*                    shape drawing (hit/hurt boxes) shared with runtime debug draw
    Hurtbox/GSTN_HurtboxSetEditorToolkit.*           FAssetEditorToolkit for UGSTN_HurtboxSetDefinition
    Hurtbox/GSTN_HurtboxGenerator.*                  Physics Asset -> hurtboxes; mirroring (pure functions)
    Validation/GSTN_CombatValidators.*               UEditorValidatorBase children (section 8)
    Validation/SGSTN_ValidationPanel.*               tab "Combat Validation"
    Tests/GSTN_EditorTests_*.cpp                     Automation tests (section 11)

Plugins/GSTNCombatDebug/Source/GSTNCombatDebug/
  Public/Widgets/SGSTN_InputDisplay.h        notation + frame gaps (M1 §4), consumed presses greyed
  Public/Trace/GSTN_CombatTrace.h            trace channel and event declarations (section 9.3)
  Public/Draw/GSTN_CombatDebugDraw.h         CVars gstn.Combat.Debug.Hitboxes/Hurtboxes/Buffer, draw functions
  Private/GameplayDebugger/GSTN_GameplayDebuggerCategory.h/.cpp
```

### 3.1 Module startup

`StartupModule` registers: asset definitions, the two toolkits' tab spawners, `FGSTN_TrackEditorRegistry` built-ins,
property type layouts (`FGSTN_HitData`, `FGSTN_CombatTimelineData`), the validators (`UEditorValidatorBase` are found
by reflection, nothing to register), a `ToolMenus` entry *Window > GSTN Combat > Validation / Input Display*, console
commands (`gstn.Combat.BakeAll`, `gstn.Combat.ValidateAll`). `ShutdownModule` reverses each. All editor-only types are
inside `#if WITH_EDITOR` only where a file is also compiled elsewhere (none here: the module is Editor-only).

## 3b. Plugin and module boundaries

Owner requirement: every plugin works on its own. M5 therefore adds **two plugins, not modules inside `GSTNCombat`**:

| Plugin (folder) | Module | Allowed dependencies | Must not be depended on by |
|---|---|---|---|
| `GSTNCombatEditor` (`Plugins/GSTNCombatEditor/`) | `GSTNCombatEditor` (Editor) | plugin `GSTNCombat` (Core, GAS, NP modules), `GSTNInput`, engine editor modules; **optional** `GSTNCombatDebug` | any runtime module, `GSTNCombat*`, `GSTNInput*`, `GSTNCombatDebug` |
| `GSTNCombatDebug` (`Plugins/GSTNCombatDebug/`) | `GSTNCombatDebug` (Runtime, no Shipping) | plugin `GSTNCombat` (Core, NP), `GSTNInput`, GameplayDebugger | `GSTNCombat*` runtime modules, `GSTNInput*` (they only expose const accessors and `IGSTN_CombatTraceSink`) |

- **Enable/disable independently**: each has its own `.uplugin` (`"Plugins"`: the required ones `Enabled: true`, `GSTNCombatDebug`
  in the editor plugin with `"Optional": true`). Disabling the editor plugin leaves the game, harness and cooked data
  unchanged (it only authors data and writes `BakedSockets`). Disabling the debug plugin removes input display, debugger
  category, trace and debug draw; the editor falls back (no input display tab, no runtime draw).
- **Editor to debug without a hard link**: the editor loads `GSTNCombatDebug` dynamically (`DynamicallyLoadedModuleNames`,
  `FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>`), which exposes `CreateInputDisplay()`. Shape drawing is shared
  through pure line generation in `GSTNCombatCore` (`FGSTN_ShapeLines::Build(const FGSTN_HitShape&, FTransform)`), drawn by
  PDI in the editor and by `DrawDebugLine` in the debug module: neither plugin needs the other for drawing.
- **Pure logic stays where it can be tested without UI**: `FGSTN_TimelinePhases` (Core), validation rule functions and
  `FGSTN_ValidationReport` (a Public header of the editor plugin, no Slate), hurtbox generator and baker maths. Slate
  widgets and toolkits are the thin layer.
- **Isolation tests**: (1) build the project with `GSTNCombatEditor` disabled and with `GSTNCombatDebug` disabled (CI matrix
  and acceptance 8); (2) `GSTNCombat.Editor.*` tests need only the editor plugin plus `GSTNCombat` and run without the debug
  plugin (the debug-dependent cases skip when the module is absent); (3) a `GSTNCombat.Debug.*` test set for the debug plugin
  runs in a game-less editor context (layout function, trace sink, `Collect` on a built component); (4) a Shipping build
  contains neither plugin. Roadmap and `combat.md` §3 get the two plugin names.

## 3c. Gameplay tags

The editor and debug tooling are **readers** of tags; the simulation never depends on them. Convention (CLAUDE.md): native
tags with `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the owning plugin; ini only for designer-made input tags
(`Config/Tags/GSTNCommands.ini`), where M5 adds none.

| Tag | Declared by | Meaning, who adds/removes |
|---|---|---|
| `Combat.Hurtbox.Head`, `.Torso`, `.Arm`, `.Leg`, `.Hand`, `.Foot` | `GSTNCombatGAS` (`GSTN_CombatTags.h`, the combat tag header of M3 §3; M4 may already declare `Head`, reuse) | Hurtbox classification (M4 `FGSTN_Hurtbox::Tags`). The Physics Asset generator suggests them from bone names (mapping in editor settings); designers add more in their own ini. Side (left/right) is not a tag: it is the bone name, used by mirroring. Never granted to an actor. |
| `Test.Combat.Editor.*` (test events/tags for editor test content) | `Config/Tags/GSTNCombatEditorTest.ini` of the project's test content | Test assets only; not part of the plugins. |

Tags the tooling **reads** (owned by others, validated here): `Command.*` and `Input.Lock.Command.*` (M1/M3: cancel window
queries, slot conflicts), `Combat.Relationship.*` and `Combat.HitType.*` (M4: variants keys, `HitType`), `Combat.Weapon.Role.*`,
`Combat.State.*`, `Combat.Lock.Movement`, `Combat.Event.*`, `Input.Event.CommandExecuted` (debugger display and `Tags` track
validation). The debugger lists active `Combat.State.*` tags of the pawn read-only. Rules: the editor adds or removes no tag
on any actor; the `Tags` track editor offers the project's tags through the standard tag picker and validates that a tag is
registered (`Timeline.HitData`, `Timeline.TagsTrack` unregistered tag = error); trace and validation codes are `FName`s, not tags.

## 4. Edit target, model, undo

### 4.1 `FGSTN_TimelineEditContext`

Abstracts what is edited so widgets do not care whether the timeline is an asset or inline: `Owner` (the
`UGSTN_CombatTimeline`, or the Blueprint's CDO), `Blueprint` (inline mode), `Get()` → `FGSTN_CombatTimelineData*`
(resolved on every call, never cached across edits), `Modify()`, `NotifyChanged(Member)` (`PostEditChangeProperty`, plus
`FBlueprintEditorUtils::MarkBlueprintAsModified` inline), `IsValid()`. Inline mode resolves `Timeline` on the CDO of the
Blueprint's `GeneratedClass`; on recompile/reinstancing (`OnBlueprintCompiled`) it re-resolves, and the toolkit closes
with a message if the property is gone. Two toolkits on one target are refused (the open one is focused).

### 4.2 `FGSTN_TimelineEditorModel`

Plain C++ object, owns no UObject: `Context`, selection (set of `FGSTN_Selection { int32 TrackIndex; int32 KeyIndex
(INDEX_NONE = the whole window); EGSTN_SelectionPart Part (Body/Start/End/Key) }`), `PlayheadFrame`, `ViewRange` (frames,
visible), `bSnapToFrames` (always on: all values are integers), `bAutoKey`, `PreviewKitSkin`, `TickRate` (§7.1),
delegates (`OnDataChanged`, `OnSelectionChanged`, `OnPlayheadChanged`). All mutations are methods that open a
transaction, call `Context.Modify()`, change the data, call `NotifyChanged`, broadcast: `AddTrack(UScriptStruct*)`,
`RemoveTracks`, `MoveWindow(Track, DeltaFrames)`, `ResizeWindow(Track, Edge, Frame)`, `AddKey(Track, Frame)`,
`MoveKeys`, `DuplicateKeys(Delta)`, `PasteKeys(Frame)`, `SetKeyTransform(Track, Key, Transform)`, `SetTotalFrames`,
`SetFirstActionableFrame`, `FitTotalFramesToMontage`. The model is the only writer: widgets and gizmos never touch
the data directly (single place for clamping and transactions).

### 4.3 Undo/redo

- Every user gesture = one `FScopedTransaction` with a localized text ("Move hitbox window", "Add key"). A drag opens the
  transaction on mouse-down (first `Modify()`), applies deltas live **without** a new transaction, and ends on
  mouse-up, so one drag is one undo step (`GEditor->BeginTransaction`/`EndTransaction` held by the lane widget; a
  cancelled drag with Escape calls `CancelTransaction`).
- The toolkit is a `FEditorUndoClient` (`MatchesContext` true for the owner): `PostUndo`/`PostRedo` revalidate the
  selection (indices out of range are dropped), clamp the playhead, rebuild rows, redraw the viewport.
- Instanced track structs live inside the owner's property graph, so the transaction system snapshots them with the
  owner: no custom undo records. Copy/paste of keys goes through `FPlatformApplicationMisc::ClipboardCopy` as
  `FString` exported with `ExportText` of the key struct (survives editor restarts and works between timelines).
- Baking is one transaction (the whole `BakedSockets` map replaced) so a bad bake is undoable.

## 5. Timeline editor

### 5.1 Toolkit and layout

`FGSTN_TimelineEditorToolkit : FAssetEditorToolkit, FEditorUndoClient` (`InitEditor(Mode, InitToolkitHost, Context)`),
standalone layout `GSTN_TimelineEditor_Layout_v1`, four tabs (`RegisterTabSpawners`):

```
+-----------------------------------------------+---------------------+
| Toolbar: Skin v | Kit v | Play < > | Bake | ...|                     |
+-----------------------------------------------+   Details           |
|  Viewport (preview, gizmos, hit/hurt boxes)   |   (selected track / |
|                                               |    key, FGSTN_HitData)|
+-----------------------------------------------+                     |
|  Frame timeline: ruler / phase bar / rows     |   Frame data summary|
+-----------------------------------------------+---------------------+
```

Tabs: *Viewport*, *Timeline*, *Details* (an `IDetailsView` set to a transient proxy object, below), *Frame Data*
(summary and the validation findings of this timeline). For an asset the toolkit's `GetObjectsCurrentlyBeingEdited`
returns the asset; for inline mode it returns the Blueprint (so Save saves the Blueprint).

Details proxy: selecting a track or key sets the details view to a `UGSTN_TimelineSelectionProxy` (transient UObject
with an `FInstancedStruct`/pointer to the track and a `Key` index) whose customization shows only the selected
struct (a hitbox track's spec, a cancel window's query and condition...) through `IStructureDetailsView`; edits come
back through `SetStructureData` and the model's `ApplyTrackEdit` (transaction, clamp, notify). `FGSTN_HitData` gets a
property type customization (`GSTN_HitDataCustomization`): damage/knockback grouped, an **angle dial** (drag the arc,
shows the resulting direction on the movement plane), special angle and yaw source with a short description each, hit
type as a tag combo, a *Preview knockback* line that calls the active `UGSTN_KnockbackModel` with a test victim
(percent, weight fields in the toolbar) **(M4 result: needs the model API as specified in M4 §3)**.

### 5.2 Frame timeline (Slate)

`SGSTN_TimelinePanel` = `SVerticalBox`: `SGSTN_TimelineRuler`, `SGSTN_PhaseBar`, `SListView<TSharedPtr<FRowData>>`
of `SGSTN_TimelineTrackRow` (fixed 200 px name column with track icon, name, mute/solo-preview toggle and "+" for
adding keys; lane to the right). A shared `FGSTN_TimelineViewTransform { double FirstFrame; double PixelsPerFrame; }`
(owned by the model) converts frame ↔ pixel for ruler, lanes and phase bar; scroll with the wheel, zoom with
Ctrl+wheel around the cursor, `Home` frames the whole timeline (`F` frames the selection). Ruler: frame ticks (labels
adapt to zoom: every frame at high zoom, every 5/10 at low) and a second line in milliseconds (`frame × 1000 / TickRate`,
one decimal), the FAF and Total markers as vertical lines.

- **Playhead**: red line, draggable on the ruler; `,` / `.` step one frame back/forward, `Shift+,` / `Shift+.` jump to the
  previous/next key or window edge, `Home`/`End` first/last frame, `Space` plays at the real tick rate (a
  `FTSTicker` advancing frames, looped, honouring *Play range*).
- **Lane painting** (`OnPaint`, no child widgets per key: 40 keys × 12 tracks stay cheap): window = rounded bar from
  `StartFrame` to `EndFrame` inclusive with the track colour from its `IGSTN_TrackEditor`; keys = diamonds at the key
  frame; selected = highlighted outline; keys inside the playhead frame are drawn bigger; invalid windows (outside
  `[0, TotalFrames)`) hatched red.
- **Interaction**: click selects; drag the bar body moves, drag the bar ends resize (cursor changes), drag a diamond
  moves the key (snapped to integer frames), `Ctrl+drag` duplicates, box select in empty lane space, `Delete` removes,
  `Ctrl+C/V` copy/paste keys at the playhead, double-click on an empty lane position adds a window (default length from
  the track editor) or a key. Frames are integers everywhere; drags clamp to `[0, TotalFrames - 1]`.
- **Phase bar** (derived, read-only): startup, active, recovery, actionable, from `FGSTN_TimelinePhases::Compute`
  (§5.3); hovering shows frame counts; the FAF marker is draggable (it writes `FirstActionableFrame` through the model).
- **Row types** come from the track registry (§12). Built-ins: Hitbox (one lane per `FGSTN_HitboxSpec`, label from
  `HitGroup` and `Space`), HurtboxModifier (colour by `EGSTN_HurtState`), CancelWindow (label = the command tags of the
  query, condition icon), Movement, Tags, Event (diamond), Charge, WeaponSwitch, and a generic fallback row for any
  unknown `FGSTN_TimelineTrack` child (window bar, name from the struct, details panel only), so a new runtime track
  type never breaks the editor.

### 5.3 Derived frame data (pure, in `GSTNCombatCore`)

`FGSTN_TimelinePhases Compute(const FGSTN_CombatTimelineData&)` → `{ int32 StartupEnd; int32 ActiveStart; int32 ActiveEnd;
int32 RecoveryEnd (= FAF); int32 Total; TArray<FIntPoint> ActiveRanges }`: active ranges = merged `[StartFrame,
EndFrame]` of every hitbox track window (ignoring muted-by-variant ones); startup = frames before the first active
range (1-based "first hit frame" is also shown, the fighting-game convention: `ActiveStart + 1`); recovery = after the
last active frame until `FirstActionableFrame`; actionable = FAF to Total. No hitbox: the whole timeline before FAF
is "busy" (shown as a single bar). Lives in Core because the runtime debug draw and the harness use it too; if M3/M4
already added an equivalent, reuse it. The summary widget prints `Startup 7 | Active 3 | Recovery 14 | FAF 24 |
Total 30 (484 ms)` and `On shield/on hit advantage` is **not** computed (needs M6 stun values).

### 5.4 Preview scene

- `FGSTN_TimelinePreviewScene : FAdvancedPreviewScene` with one `UDebugSkelMeshComponent`. Mesh = the **skin mesh of
  the preview kit** chosen in the toolbar (kit combo lists `UGSTN_PrimaryKitDefinition` assets; skin combo lists its
  `CompatibleSkins`); the choice is remembered per asset in an editor-only property `PreviewKit`/`PreviewSkin` of
  `UGSTN_CombatTimeline` (and in `UGSTN_CombatEditorSettings` for inline mode). **(M3 result: where the skin mesh
  lives in `UGSTN_SkinDefinition`.)**
- Pose: a transient anim instance playing the timeline's montage section; **the pose is set per frame, never
  ticked in real time**: `Position = SectionStart + (Frame / TickRate) × MontagePlayRate`, evaluated on demand
  (`SetPosition`, `TickAnimation(0)`, `RefreshBoneTransforms`). The same function feeds the baker, so what you
  see is what is baked (§7.2). Without a montage the mesh shows the reference pose.
- Drawn each redraw, for the playhead frame: hurtboxes of the kit's `HurtboxSet` (colour by `EGSTN_HurtState` from the
  `HurtboxModifier` tracks active at the frame, with `ShapeOverrides` applied), hitboxes active at the frame (colour by
  `HitGroup`, selected one thicker), the interpolated key values evaluated by the **same pure function the runtime
  uses** (`FGSTN_HitboxSpec` evaluation in Core, M4), baked-socket hitboxes using the baked transform (or the live
  socket plus a stale marker when baking is missing), the root capsule, facing arrow, and optional motion trail of
  a hitbox (positions at every frame of its window, toggle). Root motion/movement track preview: *Show movement path*
  toggles a polyline integrated from the `Movement` track velocity curve (approximation; the truth is Mover).
- Viewport toolbar: lit/unlit, camera presets (side, front, 3/4), follow-root toggle, ground grid, the 3D facing toggle.

### 5.5 Gizmos

`FGSTN_TimelineViewportClient : FEditorViewportClient` with a `FWidget`: translate (`W`), rotate (`E`), scale (`R`) on
the selected hitbox key (or on a hurtbox shape in the hurtbox editor).

- `GetWidgetLocation` = the key's world position evaluated at the playhead (frame transform of its `Space` × key);
  `GetWidgetCoordSystem` = the space's rotation (local) or world (toggle, `Ctrl+`` ` ``).
- `InputWidgetDelta` converts the world delta back into the key's space (inverse of the space transform at the playhead
  frame) and calls `SetKeyTransform`. Spaces: `Root` actor transform; `BakedSocket` the socket transform at that frame
  (baked, else live); `WorldAtMoveStart` the preview origin; `Projectile` the projectile axis (identity in M5).
- **Auto-key**: with a hitbox selected and the playhead on a frame without a key, a gizmo drag creates a key at that
  frame with the interpolated values first, then applies the delta (one transaction). Auto-key off: the drag edits
  the nearest key and shows a notification. **Add key at playhead** = button + `K`; **Copy keys across frames** = select
  keys, `Ctrl+C`, move playhead, `Ctrl+V`; **Offset all keys** (Shift+drag in the lane) and **Mirror key across the
  root's YZ plane** are commands in the key context menu.
- Shape handles for sphere radius / capsule half-height / box extents are the scale gizmo mapped to `Extent` (uniform
  scale for spheres), never the actor scale.
- Mouse picking: ray vs the drawn shapes selects the hitbox key (`HitProxy` per hitbox: `HGSTN_HitboxProxy`), so a
  designer can click the hitbox in the viewport and the timeline row highlights (and vice versa).

## 6. Toolbar and commands

`FGSTN_TimelineEditorCommands : TCommands<>` (rebindable): `StepBack` `,`, `StepForward` `.`, `PrevKey`/`NextKey`
`Shift+,`/`Shift+.`, `PlayPause` `Space`, `AddKeyAtPlayhead` `K`, `ToggleAutoKey` `Shift+K`, `Delete`, `Copy`/`Paste`/
`Duplicate` `Ctrl+C/V/D`, `Translate/Rotate/Scale` `W/E/R`, `FrameAll` `Home`, `FrameSelection` `F`, `BakeSockets` `Ctrl+B`,
`ValidateNow` `F7`, `FitTotalFramesToMontage`, and view toggles (hitboxes, hurtboxes, movement path). Toolbar: skin and
kit combos, play/step, an editable current-frame field with its ms label, loop range, **Bake sockets** (yellow dot and
tooltip when stale), **Validate**, and *Add track*, a combo of every registered `IGSTN_TrackEditor` (§12).

## 7. Socket baking

### 7.1 Tick rate

`UGSTN_CombatEditorUtils::GetTickRateHz()` returns the NPP fixed tick rate the project runs with (62.5 Hz today), read
from the Network Prediction settings object (exact property per spike S2). It is **never** hard-coded, never read from
a running world, and is stored with the bake (`FGSTN_BakeInfo::TickRateHz`) so a change of project tick rate marks
every bake stale. Frame *f* occupies time `[f / Rate, (f + 1) / Rate)`; the bake samples at the **start** of the frame
(matching how the runtime applies frame *f* of a timeline: M3 §7). **(M3 result: confirm the timeline frame ↔ montage
position mapping in `PlaySyncedMontage` and the montage blend-in; the bake must replicate it.)**

### 7.2 Algorithm (`FGSTN_SocketBaker::Bake(Timeline, PreviewMesh, Rate) → FGSTN_BakeResult`)

1. Sockets to bake = distinct `Socket` names of every `FGSTN_HitboxSpec` with `Space == BakedSocket` in the tracks (and
   `HurtboxModifier` shape overrides that name one). Empty set: clear `BakedSockets`, done.
2. For each socket, check that it exists on the preview mesh/skeleton (socket or bone); missing = error finding, bake
   aborted with a message naming the hitbox.
3. For `f` in `[0, TotalFrames)`: position the preview pose (§5.4), read the socket's **component-space** transform
   (`GetSocketTransform(Name, RTS_Component)`), convert to **root space** by composing with the mesh component's
   relative transform inside the character (the kit's mesh offset, e.g. `(0, 0, -HalfHeight)` and the -90 degree yaw
   of the mannequin convention), so `Root` and `BakedSocket` keys share one frame of reference; store as `FTransform`
   with scale 1 (hitbox scale comes from the key, not the bone).
4. If the montage has root motion, the evaluated pose already excludes it (spike S1 must confirm) and the baked
   transform is relative to the root **at that frame**; the runtime applies it on the root transform that Mover has
   at the frame (M4 §6.1). The bake therefore needs no root-motion information.
5. Quantize through the same function the runtime uses to read it (float → stored `FTransform` as is; if M4 quantizes
   baked transforms to save memory, round here too, so editor view = runtime).
6. One transaction: `BakedSockets` (TMap) replaced, `BakeInfo` updated, `NotifyChanged`.

Frames are independent (no state carried), so the bake is deterministic for identical inputs and the loop can run
across threads later. A 60-frame timeline with 3 sockets bakes in a few milliseconds (spike S1 measures it).

### 7.3 Staleness (`FGSTN_BakeInfo`, editor-only)

New **editor-only** field on `FGSTN_CombatTimelineData` (`#if WITH_EDITORONLY_DATA`): `FGSTN_BakeInfo { uint32 InputHash;
float TickRateHz; FSoftObjectPath Mesh; FDateTime When; }`; the only change M5 makes to an M3/M4 type, stripped in
cooked builds (`BakedSockets` itself stays: the runtime needs it). `InputHash` = hash of: montage package GUID
(`UAnimSequenceBase::GetRawDataGuid` per underlying sequence of the section), section name and start, play rate,
`TotalFrames`, tick rate, preview mesh path and its skeleton GUID, the sorted socket list. Checked on timeline load in
the editor, on toolbar refresh (cheap: GUIDs only), and by the validator; mismatch sets the stale flag and the
toolbar dot. **Bake all** (`gstn.Combat.BakeAll`, Tools menu, content browser context menu on a folder) bakes every
stale timeline asset (skips Blueprint inline ones that are not loaded; loads them under a slow task with progress).
*Bake on save* (preference, default off) runs the bake for the saved asset.

## 8. Hurtbox set editor

`FGSTN_HurtboxSetEditorToolkit` (asset: `UGSTN_HurtboxSetDefinition`): same preview scene, a tree/list of hurtboxes
(`FName`, bone, tags), details, gizmos on the selected shape, mirror of selection, undo as §4 (context = the asset).
Needs a mesh: editor-only `PreviewMesh` (soft pointer) on the hurtbox set, falling back to the preview kit's skin.

- **Generate from Physics Asset** (`FGSTN_HurtboxGenerator::FromPhysicsAsset(UPhysicsAsset*, USkeletalMesh*, Options)`,
  pure and test-covered): for each body setup in `SkeletalBodySetups` and each primitive of its `AggGeom`: sphyl → Capsule,
  sphere → Sphere, box → Box, convex/tapered capsule → the bounding capsule along its longest axis with a warning; shape
  transform = bone reference-pose transform in component space × primitive local transform × mesh relative transform,
  expressed root-relative (same frame as §7.2 step 3). Name = bone name (`bone_N` for several primitives); a
  `Combat.Hurtbox.*` tag is suggested from a mapping table in the settings (e.g. `head` → `Head`) and left empty if
  unmapped. Options: skip bodies with `PhysicsType == Kinematic`, scale margin (default 1.0), merge small bodies below
  a radius. Regeneration keeps hurtboxes edited by hand (matched by name) unless **Replace all** is chosen.
- **Mirror L↔R**: pairs bones by a naming rule from the settings (`_l`/`_r`, `Left`/`Right`, `.L`/`.R`; default list),
  mirrors the shape through the root's YZ plane (X negated in root space; the mannequin facing convention must be
  stated in settings: *forward axis*), creates the counterpart if missing, overwrites it if present (confirmation).
  Unpaired bones are reported, never guessed.
- Findings shown live: duplicate names, empty shape extents, shape outside a sane radius of the root, hurtbox on a bone
  that does not exist in the preview mesh.

## 9. Validation

### 9.1 Mechanism

`FGSTN_ValidationReport { TArray<FGSTN_ValidationFinding { EGSTN_Severity Severity (Error/Warning/Info); FName Code;
FText Message; TWeakObjectPtr<UObject> Asset; FGSTN_FindingLocation Where (track index, key index, frame); }> }`.
Pure rule functions fill a report (no UObject lookups beyond the data passed in), so they are Automation-testable.
Two surfaces use the same rules:

- **`IsDataValid`** (`UEditorValidatorBase` children in the editor module, run by Data Validation on save, on demand and
  in CI `-run=DataValidation`): `UGSTN_TimelineValidator`, `UGSTN_HurtboxSetValidator`, `UGSTN_KitValidator`,
  `UGSTN_WeaponValidator`, `UGSTN_LoadoutPresetValidator`, `UGSTN_AbilityTimelineValidator` (inline timelines on
  `UGSTN_GA_BaseCombat` Blueprints, through the CDO). Errors invalidate the asset, warnings do not.
- **Validation panel** (`SGSTN_ValidationPanel`, nomad tab *Window > GSTN Combat > Validation*): a tree grouped by asset
  with severity icons, double-click opens the asset and, for timeline findings, selects the track/key and sets the
  playhead (`FGSTN_FindingLocation`); *Validate all* runs the validators over the Asset Registry (project plugin and
  game content); results also go to the Message Log category `GSTNCombat`. The timeline editor's *Frame Data* tab shows
  the findings of the open timeline continuously (debounced 250 ms after an edit) and the lanes draw a red/yellow
  marker at the offending frame.

### 9.2 Rules

Timeline (codes `Timeline.*`; error unless noted):

| Code | Rule |
|---|---|
| `FrameRange` | `0 ≤ StartFrame ≤ EndFrame < TotalFrames` for every window; `FirstActionableFrame ≤ TotalFrames`; `TotalFrames ≥ 1`. |
| `MontageLength` | Warning: montage section length at the play rate ≠ `TotalFrames` ± 1 frame (M3 §6.3); fix action in the panel. |
| `OverlapSameKind` | Overlapping windows of the same exclusive kind: two `Movement` tracks (the second would replace the layered move), two `HurtboxModifier` tracks affecting the same hurtbox with different states, two `Tags` windows granting contradictory lock sets (warning). `Hitbox` overlap is allowed (multi-hit) but two hitboxes in the **same `HitGroup`** overlapping in time with the same shape is a warning. |
| `RootMotionMix` | Warning (M3 risks): a root-motion montage and a `Movement` track without an explicit `MixMode`. |
| `DeferredCommit` | No hitbox, event, movement track, projectile or cost before `DeferredCommitFrame` (= `OverrideFrames`, M3 §6.3). |
| `CancelWindowTarget` | `IntoCommands` empty (error), or matches no command of any kit that references the ability (warning, needs the registry scan). `OnHit`/`OnBlock` windows on an ability with no hitbox (warning). |
| `BakedStale` | Error: `BakedSockets` missing or its `BakeInfo.InputHash` ≠ the current inputs for a timeline with `BakedSocket` hitboxes. Info: baked sockets present but no hitbox uses them. |
| `BakedSocketMissing` | Hitbox names a socket absent from `BakedSockets` or from the preview skeleton. |
| `BakedLength` | `BakedSockets[socket].Num() ≠ TotalFrames`. |
| `HitboxKeys` | Hitbox keys not sorted by frame, key frame outside the window, no key at all (a hitbox needs ≥ 1), non-positive extent, `HitGroup` collisions across hitboxes that are not meant to hit separately (warning, only when the second has a different `Data`). |
| `HitData` | Negative damage/knockback, `FixedKnockback ≥ 0` together with growth (warning), `Priority` outside the clank range, unknown `HitType` tag, `CueTag` not registered. |
| `TagsTrack` | `Tags` track or `CueTag`/`GameplayEventTag` naming an unregistered tag. |
| `HurtState` | Armor with `ArmorThreshold ≤ 0`; `AffectedHurtboxes` naming a hurtbox absent from the kit's set. |
| `Variants` | `RelationshipVariants` key is not under `Combat.Relationship.*`; `ShapeScale ≤ 0` with `bIgnore` false. |
| `AimTurn` | `TurnUntilFrame` beyond `TotalFrames`, `MaxTurnDegrees` ≤ 0 for a turning policy. |
| `UnknownTrack` | Warning: a track struct with no registered `IGSTN_TrackEditor` (shown by the generic row). |

Hurtbox set: duplicate names, extents ≤ 0, bone absent from the preview mesh (warning, mesh optional), `Combat.Hurtbox`
tag outside the root, no hurtbox at all (error).

Kit/weapon/loadout preset (UI over existing M1/M3 validators, no new logic): the M1 slot validator (`FindConflicts`:
`Ambiguous`, `DuplicateCommand`, `InvalidSlot` errors, `Shadowed` warnings) run over the compiled command set of each
kit, each weapon on top of the kit, and each preset (`UGSTN_LoadoutDefinition`), plus `UGSTN_LoadoutRules` of M3 §8
(allowed weapons/supers/skins, one stats GE per weapon, bindings to removed commands, timelines the kit can trigger
present in the registry). Each conflict finding carries both slots and a **Show** action that lists their patterns in
the notation of M1 §4; a conflict on a **preset** names the weapon or super that introduces it. Findings appear on the
kit/weapon/preset assets (so the Content Browser shows the validation badge) and in the panel.

## 10. Debug

### 10.1 Input display (`SGSTN_InputDisplay`, `GSTNCombatDebug`)

A scrolling strip fed by the combat component's `FGSTN_InputHistory`, newest at the right, **one column per frame** at the
NPP tick rate: row 1 buttons (press/hold/release), row 2 the stick as numpad digit (physical) with the facing-relative
direction beneath, flick `!`. The **frame gap** between events is printed in the M1 §4 notation (`7:+A`); presses consumed
by a command are **greyed** (command tag on hover) and a command execution (`Input.Event.CommandExecuted`) draws a marker.
Shown as a PIE corner overlay (`gstn.Combat.Debug.InputDisplay 1`) and as an editor tab. It can also show a **scripted**
history (`FGSTN_InputScript::Build`): type a notation, see which command of the open kit resolves ("what does this input
do"). The layout is a pure function (model tests, §13).

### 10.2 Gameplay Debugger category (`FGSTN_GameplayDebuggerCategory`)

Registered in `GSTNCombatDebug` (`WITH_GAMEPLAY_DEBUGGER_MENU`, default key `6`, configurable); collects on the authority
for the selected pawn and replicates through the category data pack, so a dedicated server PIE works. Shows: combat state
(action state, active timeline with frame/FAF/total, hitstop and hitstun remaining, active weapon, loadout revision, lock
tags, adoption pending); input (last N history events in notation, buffered command, last resolved command and why);
active windows now (cancel, override, `Tags`, hurt state); the last 8 hits dealt and received (frame, other entity, hit
data id, damage, knockback, relationship), from a small ring in the debug module fed by `UGSTN_SimEventSubsystem::Listen`
(never sync state); world draw of boxes, facing arrow, timeline frame above the head. `UGSTN_CombatSimComponent` gains
**read-only const accessors** for this (history, timeline frame, windows, registry names), the only change to M2/M3 types
besides §7.3; they never run inside the simulation.

### 10.3 Debug draw and CVars

`gstn.Combat.Debug.Hitboxes/.Hurtboxes/.Buffer/.InputDisplay` (combat.md §14). `GSTN_CombatDebugDraw` draws the **same
shapes** as the preview (the drawing code lives in `GSTNCombatDebug`, shared by the editor) from the world snapshot (M4 §5),
so the picture is the simulated truth: hitboxes by group, hurtboxes by state, contact points for 0.5 s; non-shipping only,
on the machine that simulates. `gstn.Combat.Debug.BakeCheck` overlays baked socket data on the live socket (§15).

### 10.4 Trace of the combat state

`GSTN_CombatTrace`: channel `GSTNCombat`; `UE_TRACE_LOG` event `CombatFrame` per simulated frame per entity (entity, frame,
resimulated flag, action state, timeline index and frame, hitstop, hitstun, history head) and `CombatEvent` (command
executed, hit, adoption, cancel, rollback frame range, with `TRACE_BOOKMARK` for rollbacks). `GSTNCombatNP` emits through a
tiny interface (`IGSTN_CombatTraceSink`, set by the debug module, null in shipping), so NP never depends on the debug
module. v1 view: Unreal Insights plus the Gameplay Debugger. **M5.8 (stretch, spike S3):** a Rewind Debugger extension
with a track per entity showing timeline and state transitions, re-simulated frames hatched.

## 11. Settings

`UGSTN_CombatEditorSettings` (`UDeveloperSettings`): default preview kit/skin, bake on save, auto-key default, track
colours, hurtbox mirror naming pairs and forward axis, Physics Asset → `Combat.Hurtbox.*` tag mapping, debugger key.
Project-wide values (naming pairs, mapping) go to `DefaultEditor.ini`, per-user ones to user settings.

## 12. How to extend

**Add a new track type to the editor** (e.g. M6's `Shield` window): 1) the runtime struct `FGSTN_ShieldTrack :
FGSTN_TimelineTrack` exists in its runtime module (M3 §6.3 rules). 2) In the editor module implement
`IGSTN_TrackEditor` (`Timeline/GSTN_TimelineTrackEditor.h`):

```cpp
class IGSTN_TrackEditor
{
public:
    virtual UScriptStruct* GetTrackStruct() const = 0;
    virtual FText GetDisplayName() const = 0;
    virtual FLinearColor GetColor() const = 0;
    virtual bool HasKeys() const { return false; }               // window only, or window + diamonds
    virtual void InitNewTrack(FGSTN_TimelineTrack& Track, int32 PlayheadFrame, const FGSTN_CombatTimelineData&) const {}
    virtual FText GetRowLabel(const FGSTN_TimelineTrack&) const;  // text on the bar
    virtual void DrawPreview(const FGSTN_TrackPreviewContext&) const {}  // viewport primitives for the playhead frame
    virtual void Validate(const FGSTN_TimelineTrack&, const FGSTN_CombatTimelineData&, FGSTN_ValidationReport&) const {}
};
```

3) `FGSTN_TrackEditorRegistry::Get().Register(MakeShared<FGSTN_ShieldTrackEditor>())` in the module's startup (or in
the extending plugin's editor module). It then appears in *Add track*, in the lanes, the details proxy and the
validator with no other change. Tracks with gizmos also implement `IGSTN_GizmoProvider` (returns widget location and
applies deltas through the model). 4) Add a test (§13: a validate case and a model case).

**Add a validation rule**: a pure function `void Check<Rule>(const FGSTN_CombatTimelineData&, FGSTN_ValidationReport&)`
in `GSTN_CombatValidators.cpp`, add its code to the table of §9.2 and a positive/negative test. **Add a debugger
section**: a `Collect` block in the category and a `Draw` block; read data only through the const accessors.
**Add an editor command**: declare in `FGSTN_TimelineEditorCommands`, map in the toolkit's `BindCommands`, route the
mutation through the model (never write data from a widget).

## 13. Tests and harness

Editor Automation tests (`WITH_DEV_AUTOMATION_TESTS`, `EAutomationTestFlags::EditorContext | ProductFilter`), named
`GSTNCombat.Editor.<Area>.<Case>`, run like the others (testing.md; add `Automation RunTests GSTNCombat.Editor` to the
command line; they build no viewport, so `-nullrhi` works).

| Area | Cases |
|---|---|
| `Model` | Each mutation changes only what it says and clamps to `[0, TotalFrames)`; undo/redo restores the exact data for every mutation (`GEditor->UndoTransaction` on a transient asset), a drag is one step; selection revalidated after undo; key copy/paste round trip through text; auto-key creates the key with interpolated values. |
| `Phases` | `Compute`: no hitbox, one, two disjoint windows, overlapping merged, hitbox at frame 0, FAF = Total. |
| `Bake` | Baked transform at frame *f* equals the pose at `f / TickRate` within 1e-3 cm; frame count = `TotalFrames`; two bakes bit-identical; tick rate, play rate or socket list change marks stale; a missing socket is an error and keeps the previous bake. |
| `Hurtbox` | `FromPhysicsAsset` on a test Physics Asset: counts per primitive type, convex fallback capsule, root-relative positions; regeneration keeps hand edits; mirror creates the counterpart and reports unpaired bones. |
| `Validate` | Every code of §9.2, one failing and one passing timeline built in code; a kit with a conflicting slot is flagged with both slot names. |
| `Registry`/`Debug` | A registered test track editor is listed; an unknown track gets the generic row and `UnknownTrack`; input display layout of `0:face=90 0:4t 6:6t! 7:+A` (columns, gaps, greyed press); debugger `Collect` writes the expected fields (non-world part). |

Test content under `/Game/GASNPPTest/Combat/Editor/` (a montage and skeleton shared with M3/M4 content, a Physics Asset,
`TL_Test_Jab3`); nothing here edits `BP_GASNPP_TestCharacter`. A helper `Tools/GASNPPTest/gstn_editor_check.py` (editor
Python console) opens a timeline, bakes, validates and prints the report, for the manual smoke test. PIE harness: no new
scenario; `scenario_hit_basic` of M4 is re-run with a timeline **authored in the editor** (acceptance 6).

## 14. Acceptance criteria

1. Spikes S1–S4 recorded in combat.md §17; M5 build steps below each end with a clean build and tests.
2. **The designer scenario, editor only** (no details-panel frame editing, no Python): build a three-hit jab string, each
   with a hitbox (one `Root`, one `BakedSocket` on the weapon hand, one with an ally variant), a cancel window from
   jab 1 into jab 2 on `OnHit` and `Always`, a `HurtboxModifier` armor window on jab 3, and an EX version (a second
   ability with the EX command and a shared timeline asset with a different hitbox size); frame data shows startup /
   active / recovery / FAF; Undo/redo restores every step.
3. Deliberately conflicting slot (two kit slots matching the same canonical input) is flagged as `Ambiguous` on the kit
   and on the loadout preset, with both slot names and notation; the fix clears the error.
4. Baking: baked frames equal the preview pose within 1e-3 cm; changing the montage, play rate, sockets or tick rate
   shows the stale dot and the `BakedStale` error; *Bake all* clears all of them; cooked builds contain no
   `FGSTN_BakeInfo`.
5. Hurtbox set generated from the Physics Asset of the test character, mirrored, edited with gizmos; counts match
   the bodies; regeneration keeps hand edits.
6. A timeline authored in the editor and the equivalent hand-built one give identical `scenario_hit_basic` snapshots
   (M4 harness, 60/15 ms, 2 % loss).
7. Inline mode: the same flow on the inline timeline of a `UGSTN_GA_BaseCombat` Blueprint; Blueprint marked modified,
   survives compile, Save writes the Blueprint.
8. Input display and Gameplay Debugger show correct data in a PIE with dedicated server and two clients (server's
   category shows the client pawn); the trace channel can be captured in Insights. No Core/GAS/Input module depends on
   `GSTNCombatEditor` or `GSTNCombatDebug` (verified by the module `.Build.cs` files and a packaged Development
   build, the editor module absent from it).
9. A new track type added by the procedure of §12 in under one hour without touching widget code (done once with a
   throwaway track).
10. Editor tests (`GSTNCombat.Editor.*`) green; M1–M4 tests and scenarios still clean; combat.md §3, §14, §17 and the
    plugin README updated.

Suggested build order: M5.1 module skeleton + model + undo tests; M5.2 timeline panel + phases; M5.3 preview scene +
gizmos; M5.4 baking; M5.5 hurtbox editor; M5.6 validators + panel; M5.7 input display + debugger category + debug draw;
M5.8 trace and (if S3 allows) Rewind Debugger.

## 15. Risks

- **Custom Slate cost**: timeline interaction (zoom, box select, snapping, drag undo) is the largest item and the one
  designers touch hourly. Mitigation: model/view split (§4.2); a usable cut after M5.2 with details-panel fallback.
- **Montage evaluation fidelity** (S1): the editor's sampling must equal `PlaySyncedMontage` at runtime (blend-in, notify
  driven poses, layered anim instances, post-process). The acceptance test compares to the preview, and
  `gstn.Combat.Debug.BakeCheck` shows any divergence in PIE.
- **Gizmos on non-components** (S4): `FEditorViewportClient` assumes actors/components; virtual selection may need hit proxy
  workarounds that vary between engine versions; keep that code in one class.
- **Inline timelines in Blueprints**: CDO edits and reinstancing. Fallback if brittle: inline mode becomes a read-only
  preview with "extract to a timeline asset" (asset timelines are the recommended workflow anyway).
- **Dependency drift**: `StructUtils`, `AdvancedPreviewScene`, Rewind Debugger and Gameplay Debugger APIs change between
  5.x; verify against the 5.8 source before each step. **Data model drift** with M3/M4: the editor touches only the public
  structs and pure evaluation functions, always behind the model and track editors.
- **Editor-only field in a runtime struct** (`FGSTN_BakeInfo`): guard `WITH_EDITORONLY_DATA` in `NetSerialize`/copy code
  and run Core tests in a cooked-like configuration. **Debug module leaks into Shipping** (`UE_TRACE`, debugger macros): the
  Shipping build is in the acceptance list.

