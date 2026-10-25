# GSTNCombatEditor

Editor tooling of GSTN Combat for Unreal Engine 5.8: the timeline editor (edit context, model, undo, the timeline panel, the
toolkit and track editors, the preview scene, then gizmos), validation, socket baking and the hurtbox set editor. A designer builds a move with windows and keys instead
of typing frame numbers in a details panel. Everything the tooling writes is the plain data of `GSTNCombatCore`
(`FGSTN_CombatTimelineData`, the hurtbox data): the runtime never knows the editor exists, and assets stay editable in the
details panel.

Status: **experimental (0.13)**, `EnabledByDefault: false`, editor only. Checkpoints E1 (skeleton), E2 (edit context and
model), E4 (timeline panel), E5 (toolkit, track editors), E6 (preview scene), E8 (socket baking), E9 (hurtbox set editor), E10 and E11 (validation), E13 (inline mode) and E14 of [m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md)
(E3, `FGSTN_TimelinePhases`, is in `GSTNCombatCore`; E14, the input display, lives in `GSTNInputDebug` (plugin GSTNInput) with its combat adapter in `GSTNCombatDebug`); the design is
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
| `FGSTN_PhaseBarData` | `GSTN_PhaseBarData.h` | Startup, active, recovery and actionable stretches, the tail and the summary line. The bar's view model: segments laid out from `FGSTN_TimelinePhases` (E3, `GSTNCombatCore`), which owns the numbers and the summary line. |
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
| Bake Sockets | `Ctrl+B` | mapped by the toolkit (E8) |
| Show Hitboxes/Hurtboxes/Movement Path | none | declared, mapped when the overlays of E7 exist |

### The timeline editor window (E5)

Double-click a `UGSTN_CombatTimeline` asset in the Content Browser (a Data Asset of that class, as before): the asset definition
`UGSTN_AssetDefinition_CombatTimeline` (`UAssetDefinitionDefault`) opens `FGSTN_TimelineEditorToolkit`, a standalone asset editor with
four tabs in the layout `GSTN_TimelineEditor_Layout_v1`: **Viewport** (the 3D preview, E6, below), **Timeline** (the
panel of E4 under a warning bar), **Details** and **Frame Data**. Save is the editor's own Save; the layout is saved and restored by
the editor. A second toolkit on an open asset is refused and the open one comes to the front (the context registry of E2).

| Type | File | Role |
|---|---|---|
| `FGSTN_TimelineEditorToolkit` | `Private/Timeline/GSTN_TimelineEditorToolkit.h` | Owns the model, the undo client and the lease of the target; spawns the four tabs. |
| `UGSTN_TimelineSelectionProxy` | `Private/Timeline/GSTN_TimelineSelectionProxy.h` | A transient copy of the selected track, shown by a structure details view. `Commit` writes it back with `ApplyTrackEdit` (one undo step; a slider drag is one step through the model's drag). |
| `SGSTN_TimelineDetails` | `Private/Timeline/Widgets/` | The Details tab: follows the selection (one row at a time) and the data. |
| `SGSTN_MontageLengthBar` | same | Warning bar when the montage and `TotalFrames` disagree, with **Fit Total Frames to montage** (undoable, explicit, never automatic). |
| `SGSTN_TimelineToolbarExtra` | same | Loop range (editor state, not saved), the preview subject picker (E6), Validate (runs the timeline rules, E10). |
| `IGSTN_TrackEditor`, `FGSTN_TrackEditorRegistry`, `IGSTN_GizmoProvider`, `FGSTN_TrackPreviewContext` | `Public/Timeline/GSTN_TimelineTrackEditor.h` | The extension point (below). |
| `GSTNTrackEditors` | `Private/Timeline/GSTN_TrackEditors.cpp` | The built-in track editors: CancelWindow, Movement, Tags, Event, WeaponSwitch, and (E5b) Hitbox and HurtboxModifier. |
| `FGSTN_HitboxKeyedAccess` | `Private/Timeline/GSTN_HitboxKeyedAccess.h` | The keyed adapter of the hitbox track (E5b), below. |

The names, colours, icons, bar labels and default lengths of the rows come from the track editors. A track struct nobody registered
(a game's own track) gets the **generic row**: a window bar named by the struct,
its fields in the Details tab. Add Track lists the registered editors, then "Other track types" for the unregistered structs.

### The preview scene (E6)

The Viewport tab (`SGSTN_TimelineViewport`) is a 3D preview of the timeline at the playhead frame: `FGSTN_TimelinePreviewScene` (an `FAdvancedPreviewScene`)
with one `UDebugSkelMeshComponent`, `FGSTN_TimelineViewportClient` (camera presets Side, Front and 3/4, Frame, Follow root, Grid, Root capsule and facing
arrow, Lit/Unlit) and `FGSTN_PreviewController`, which the toolkit creates next to the model and which keeps the scene in step with the playhead, the
montage, its section and play rate. The preview loads the timeline's montage by itself (the one place besides *Fit Total Frames* that does).

**The subject is generic.** `FGSTN_PreviewSubject` is either a **player kit and skin** (read only: the skin's mesh and animation class, the kit's
locomotion class) or **any `USkeletalMesh`** (an enemy, humanoid or not, with an optional animation class), plus a list of **attachments**: a skeletal or
static mesh hung from a bone or socket of the main mesh with a full relative transform, scale included (a sword in a hand, a turret on a base; a
longer blade is a scale along one axis). Everything is a soft reference. The toolbar's **Preview** combo opens the subject's properties: kind, assets,
the attachment list. No weapon data names a mesh yet, so attachments are added by hand for both kinds of subject; when a weapon definition gets a mesh
the resolver (`GSTNPreviewSubject::Resolve`) is where it joins.

The subject is stored **per asset** (the timeline asset, or the ability Blueprint for an inline timeline) in `UGSTN_CombatEditorPreviewState`, config
`EditorPerProjectUserSettings`: per user and project, never in the versioned asset (`UGSTN_CombatTimeline` is a runtime type and has no preview
properties). A restart keeps it; deleting the entry in that ini resets it.

**Nothing assumes a skeleton.** The mesh is placed in root space (the space hitboxes live in: +X forward, Z up, origin at the middle of the capsule)
from the mesh alone: turned so `USkeletalMesh::ForwardAxis` faces +X, dropped so the middle of its reference-pose bounds sits at the origin's height. The
root capsule is derived from the same bounds (the narrower horizontal half-size as radius, the half height from Z) and the facing arrow is +X. Bones and
sockets are only ever asked for by the name the caller holds.

**The pose is set, never ticked.** `FGSTN_PreviewPose::Evaluate(Frame)` puts the montage at `SectionStart + Frame / TickRate x PlayRate` on the debug
component's preview instance (paused, no blend-in or blend-out) and evaluates once with a zero delta; the baker (E8) will call the same function. No
montage: the reference pose. Spike S1 (docs/notes/m5-spike-report.md) measured it: bit-identical across runs and backwards, the root bone's track (root
motion) is not in the pose, the post process graph changed nothing it sampled.

Overlays that need M4 (the hit and hurt shapes) register in `FGSTN_PreviewOverlayRegistry` (`IGSTN_PreviewOverlay`): the viewport calls every registered
one at the playhead frame, none is built yet. Not in this checkpoint: the movement-path polyline and the motion trail (both toggles of the spec), the
skin combo listing only the kit's compatible skins (the subject lists any skin and says in words when it does not match the kit's query), and gizmos (E7).
Camera: a plain camera, not the engine's orbit camera, whose stored rotation is read through its own orbit convention; Alt+drag orbits.

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

### The Hitbox and HurtboxModifier rows (E5b)

**Hitbox.** One row per track (one window, several hitboxes). The bar is labelled `Part: G0, G1 @hand_r sweet` (the part or group, then each hitbox by its `HitGroup`, where its
space is and its zone; more than three end in `+N`; the full text is the tooltip of the name column) and the label is drawn where no key covers it. The keys of each hitbox are
diamonds in their own **sub-lane** of the row (one lane per `FGSTN_HitboxSpec`, a little smaller than the keys of a one-lane track), and a click picks the key
nearest in x and then in y, so two hitboxes with a key on the same frame are told apart by the height of the click. A double click on a lane adds a key to **the lane under the cursor**;
`K` and the `+` of the name column add to the lane of the selected key (the first lane when nothing is selected); paste and Ctrl+drag copies go to the lane they came from. A key added on
a path takes the values the runtime evaluates there (`UGSTN_HitShapeUtils::EvaluateKeys`, the function the hit code reads), so adding a key never changes the path. A track with no hitbox gets
one with its first key; a new track starts with a default sphere in front of the root. The part, shape, space, socket, group and hit data are edited in the Details tab (the hit data
customization is E12).

`FGSTN_HitboxKeyedAccess` is the adapter: key indices are flat over the track (the keys of hitbox 0, then hitbox 1, ...), each lane sorted by frame, and the frame the model sees is
absolute (`FGSTN_HitboxKey::Frame` counts from the track's first frame, so moving the window moves its keys without re-keying). `IGSTN_KeyedTrackAccess` got three
defaults for it: `GetLaneCount`, `GetKeyLane` and `AppendKeyInLane`; a caller that appends several keys does it in ascending lane order.

**HurtboxModifier.** A window coloured by the state it gives (`GSTNTimelineColors::HurtStateColor`: invincible blue, intangible violet, armor amber; the track editor's new
`GetTrackColor(Track)` lets a colour depend on the data) and labelled `Armor >= 30: Torso, Arm_R` (the state, its threshold, then the hurtboxes it addresses by part, name or tag).

### The hit data row and the knockback preview (E12)

`FGSTN_HitDataCustomization` (`IPropertyTypeCustomization`, registered for `FGSTN_HitData` by the module) replaces the flat list of a hit's fields wherever a hit is edited: the
Details tab of the timeline editor for a selected hitbox, the values of a relationship variant, a hit in any asset. The row opens by itself and shows a one-line summary
(`Damage 6, knockback 400 + 10/dmg, 35 deg`). Groups: **Damage** (damage, shield damage), **Knockback** (base, growth, fixed, the **angle dial**, angle, special angle, yaw source), **Hitstun**, **Hitstop**,
**Stagger**, **Hit** (priority, the hit type as a tag combo limited to `Combat.HitType`, extra effects, cue, custom data), **Impact**, **Extensions** (the standard array editor of the
instanced structs, so the customization of a plugin's own extension struct shows inside it) and **Preview knockback**. Damage, Knockback and the preview start open.

The **dial** (`SGSTN_AngleDial`) is a side view: the attacker at the pivot, the movement plane to the right, the elevation as the arc. Drag the needle to set `AngleDeg` (whole
degrees; Shift 5, Ctrl 15; Esc puts the angle back); a drag is **one undo step** (a transaction opened on mouse-down, interactive writes through the property handle, the last write on
mouse-up). Next to it the caption says the elevation and the direction it makes on the movement plane, `35 deg, above the plane: forward 0.82, up 0.57`. With the Sakurai special angle the
dial is dimmed and takes no drag, since the rules' angle replaces it. A sentence under *Special Angle* and *Yaw Source* says what the chosen value does. The geometry is `GSTNHitDataDial`
(`GSTN_HitDataMath.h`), the sentences `GSTNHitDataText`.

The **preview** is `GSTNHitDataPreview::Compute`: this hit on a test victim (percent, weight and *Grounded* in the row; kept for the editor session, not saved) through
`UGSTN_KnockbackModel::Compute` of the **default combat rules** (`UGSTN_CombatSettings`), with the input the pipeline's `UGSTN_HitStep_ComputeKnockback` builds (the Sakurai pre-pass, `UGSTN_HitMathUtils::LaunchDirection`, the rules' stagger threshold).
It calls the model itself, so there is no second copy of any formula. The line reads `victim at 50 %, weight 100: knockback 460 cm/s, Launch; staggers (force 6), hitstun 12 f; hitstop 5 f (attacker 5 f). Attacker faces the victim;
direction 35 deg above the plane.` The tier comes from the model's `FGSTN_ReactionTuning`. Assumed and said: the attacker faces the victim, no hurtbox scales, the hit is not in tumble. With no default rules or no model in them
the line says so. Needs the plugin `GameplayTagsEditor` (the tag combo).

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

Planned in this module, by checkpoint: gizmos on hitbox keys and hurtbox shapes (E7).

### Socket baking and staleness (E8)

A hitbox in `BakedSocket` space follows a bone or socket of the attacker's mesh, and the simulation cannot ask an animation graph where that is. So the
editor samples the montage once per timeline frame and stores, in `FGSTN_CombatTimelineData::BakedSockets`, the root-relative transform of every socket
a `BakedSocket` hitbox uses; the runtime only indexes that array. **Bake sockets** (toolbar, `Ctrl+B`) does it in one undo step.

- `FGSTN_SocketBaker` (`Private/Bake/`): `CollectUses` (the hitboxes that name a socket), `Bake(Data, Pose, Inputs)` (uses the open editor's preview pose, so what is baked is what is
  shown), `BakeStandalone(Data, Mesh, Rate)` (own scene, for Bake all and scripts), `Apply` (writes `BakedSockets` and `BakeInfo`). The sampling is the preview's: montage time
  `SectionStart + Frame / TickRate x PlayRate`, set not ticked, at the START of frame `f`; component space to root space with the mesh placement of the preview scene; scale 1.
  Frames are independent, so two bakes of the same inputs are bit-identical. A bake that cannot happen (no preview mesh, a socket the mesh lacks, no length, a montage that
  does not load) changes nothing, lists every reason in the Message Log and keeps the previous bake. No montage is allowed (the reference pose is baked, with a warning).
- Staleness: `FGSTN_BakeInfo` (`GSTNCombatCore`, `WITH_EDITORONLY_DATA`, stripped from cooked data) records `InputHash`, `TickRateHz`, the mesh path and the time. `GSTNBake::GetStatus` gives
  `NotNeeded / NeverBaked / BakedElsewhere / Fresh / Stale / Unknown` with a sentence. The hash covers the data model GUID and the segment timing of every animation the section
  plays, the section, play rate, `TotalFrames`, tick rate, the mesh path and skeleton GUID, the sorted socket list and what the mesh says about each socket. It reads GUIDs and paths only.
  The toolbar button shows a yellow dot when the frames are missing, stale or baked by another tool (the tooltip says why); the Frame Data tab and the validator report the same
  (`BakedStale` is an error for the editor's own stale bake, a warning for frames with no record). Checked when the editor opens, after each edit (debounced) and when the preview subject changes.
- **Bake on save** (setting `bBakeOnSave`, off): the toolkit bakes before it saves when the bake needs it and a preview mesh is shown.
- **Bake all**: `gstn.Combat.BakeAll [save] [/Package/Path]` bakes every timeline asset whose bake is missing or stale, taking the mesh from the asset's stored preview subject, else from its
  last bake, else from the settings' default skin (an asset with none is skipped and listed). Baked assets are left modified; `save` saves them. Timelines inline in ability Blueprints are baked from their own editor.
- `UGSTN_CombatEditorUtils` (`BlueprintCallable`, development only): `GetTickRateHz`, `BakeTimeline(Timeline, Mesh, OutMessage)`, `IsBakeStale(Timeline, Mesh)`.
- Tests `GSTNCombat.Editor.Bake.*` (11) and `GSTNCombat.Core.TimelineData.*` (2).

### The hurtbox set editor (E9)

Double click a `Hurtbox Set` (`UGSTN_HurtboxSetDefinition`) to open `FGSTN_HurtboxSetEditorToolkit`: the preview viewport of the timeline editor with the set's shapes drawn over the
mesh (the selected ones thicker and yellow), a **Hurtboxes** list (position, name, bone, zone tags, priority), the **Details** of the selected hurtbox, and the live **Findings**. Every edit goes
through `FGSTN_HurtboxEditModel` (no UI types; one transaction each; a slider dragged in the details is one undo step), so undo is the editor's.
The mesh is the set's editor-only `PreviewMesh` (saved with the set, stripped from cooked data), or the preview subject (kit and skin, or any skeletal mesh, with attachments) the timeline editor also uses. Any
skeleton works, a person or a turret: no bone name is assumed.

- **Generate from Physics Asset** (`GSTNHurtboxGenerator::FromPhysicsAsset`, pure): per primitive of every body, a sphyl is a Capsule, a sphere a Sphere, a box a Box; a convex body or a tapered capsule is the capsule that
  bounds it, with a warning. The shape is the primitive in the bone's space, then the bone's reference pose in component space, then the preview scene's placement of the mesh: root-relative, the frame the baker uses. The
  name is the bone's (`bone_N` when a bone has several primitives), the zone tag is suggested by the settings' `HurtboxZoneRules` and empty when nothing matches. Options: skip kinematic bodies (off by default: a count a designer can check by eye),
  a scale margin, merge bodies smaller than N cm into the nearest ancestor that has a bigger one (the ancestor's shape grows to contain them). Regenerating **adds missing hurtboxes** and leaves the existing ones exactly as they are
  (hand edits survive, matched by name); **Update shapes** also takes the Physics Asset shape and bone for the same names but keeps tags, priority and reaction scale; **Replace all** asks first and discards the hand-made.
  Existing hurtboxes keep their position (a hit carries the index); a hurtbox the Physics Asset does not make is kept and counted.
- **Mirror selection** (`GSTNHurtboxMirror`, pure): offered only when the skeleton has recognisable pairs under the pairs in force; the tooltip says why when it is off. The pairs are `MirrorNamePairs` of the settings or the
  `SkeletonMirrorRules` entry of that skeleton, which can also switch the skeleton off. A fragment must sit on a word boundary (`_l` pairs `arm_l`, not `arm_lower`) and keeps its case (`_L` gives `_R`). The shape crosses the plane that contains the
  forward axis (`MirrorForwardAxis`) and up, as a mirror (`M T M`, not a negative scale). The counterpart is created, or overwritten after a confirmation that names the hurtboxes. A name or bone with a side marker but no counterpart is reported in the
  Message Log, never guessed; a name with no marker is the middle and is skipped. With no mesh shown, the hurtbox names alone decide.
- **Findings**: duplicate names, empty extents, tag outside `Combat.Hurtbox`, bone absent from the preview mesh, shape outside the sane radius; a double click on one selects its hurtbox.
- Not in this checkpoint: gizmos on the selected shape (they belong to the viewport work of E7; until then a shape is edited in the details), a Physics Asset picker (the mesh's own Physics Asset is used), attachments in the set editor's own toolbar (the subject picker is the timeline editor's).
- Tests: `GSTNCombat.Editor.Hurtbox.*` (18) and the snapshot `GSTN.Visual.Hurtbox.Panel`.

## Validation (E10)

One set of rules, three readers: the **Data Validation** validators (on save, on demand, `-run=DataValidation` in CI), the **Validate**
button of the timeline editor (the findings go to the Message Log, category *GSTN Combat*, and the window is only brought up when the Frame Data tab is closed, because that tab shows the same findings), and from E11 the validation panel. The rules
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

**M4 rules (E11)**, in `Private/Validation/GSTN_HitRules.cpp`, same shape as above:

| Code | Rule | Severity |
|---|---|---|
| `HitboxKeys` | a key at or past the window length (keys count from the track's first frame); keys out of order and a shape with no volume (Core's wording, classified); two hitboxes of one `HitGroup` open together with the same `Priority`, different hit data and different shape (a tie nobody chose). Different priorities are a sweetspot, silent. A hitbox with no key is allowed (M4: the shape stays where its space puts it) | error, error, error, warning |
| `OverlapSameKind` (hitbox half) | two hitboxes of one group open together with the same shape, space, socket and key path | warning |
| `HitData` | negative damage, knockback, growth, shield damage, multipliers, stagger force or hitstop frames; `FixedKnockback` together with growth; a hit type or cue that is not registered; Core's hitstop clamp and "data only in M4" notes; for the base hit and every variant that overrides fields | error, warning, error, as Core |
| `Variants` | a variant key outside `Combat.Relationship`; `ShapeScale` 0 or less without `bIgnore`; a negative multiplier | error |
| `HurtState` | `Armor` with a threshold of 0 or less that is not infinite (this replaces Core's softer "no threshold" warning); a modifier that names a hurtbox the known set lacks (only when the caller passes `KnownHurtboxNames`) | error |
| `BakedSocketMissing` | a `BakedSocket` hitbox whose socket has no baked frames (replaces Core's warning), or names no socket | error |
| `BakedLength` | baked frames of a socket are not `TotalFrames` long (replaces Core's warning) | error |
| `BakedStale` | a baked socket no hitbox uses (info); frames baked by another tool, with no record of their inputs (warning); the editor's own bake whose inputs changed since (**error**, E8) | info / warning / error |

**Hurtbox set** (`UGSTN_HurtboxSetValidator`, `FGSTN_CombatValidators::CheckHurtboxSet`): the set's own `ValidateHurtboxSet` classified into `Hurtbox.Empty`, `.DuplicateName`, `.Extent`, `.Tag` (no tag: warning) and `.Data`, plus `Hurtbox.Tag` (error) for a tag that is not under `Combat.Hurtbox`.
Since E9 two more codes: `Hurtbox.BoneMissing` (warning: a hurtbox names a bone the preview mesh lacks; the mesh is the set's `PreviewMesh`, or the one the open editor shows, and with neither nothing is checked) and `Hurtbox.Outside` (warning: a shape further from the root than `HurtboxSaneRadiusCm`). Both point at the hurtbox by its position in `Where.TrackIndex`.

### Validate all and the validation panel (E11)

`GSTNValidateAll::Run` collects every combat asset in the Asset Registry (timelines, hurtbox sets, kits, weapons, classes that hold a
`FGSTN_LoadoutData`, and Blueprints whose generated class derives from the ability, found by the registry without loading), loads each and runs
`UGSTN_CombatValidatorBase::BuildReportForAsset`, which is the code the Data Validation validators run: the panel shows what Data Validation would.
Default scope is everything except `/Engine` and `/Temp`; a path prefix narrows it. `gstn.Combat.ValidateAll [prefix]` does the same from the console or `-ExecCmds`
and writes one page to the Message Log (category `GSTNCombat`); the panel's **Validate all** button does it with a progress dialog.

`SGSTN_ValidationPanel` (nomad tab *Window > GSTN Combat > Validation*, `GSTNValidationTab`) is a tree grouped by asset (worst severity first) with
severity icons, filters for errors, warnings and notes, and a text filter on code, message and asset name. A double click opens the asset; for a timeline finding
(an asset, or the inline timeline of an ability Blueprint) the timeline editor opens, selects the track (and key) and puts the playhead on the frame
(`GSTNValidationNavigation::OpenFinding`, `FGSTN_TimelineActions::GoToFinding`). All state is in `FGSTN_ValidationPanelModel`, which the tests read.

The timeline editor's **Frame Data** tab now follows the timeline: 250 ms after the last edit (debounced, restarted by each edit of a drag) the rules run again
silently (`FGSTN_TimelineEditorToolkit::RefreshFindings`, no Message Log), and the lanes mark each finding that has a track and frame: a thin line with a square on top,
red for an error, yellow for a warning. The Validate button / F7 still writes the Message Log.

*Bake all* and the hash half of `BakedStale` came with the socket baker (E8, below): a stale bake is now an error.

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
  data, and the `Combat.Hurtbox.*` tags the settings map to), `GSTNCombatNP` (since E13: the ability class `UGSTN_GA_BaseCombat`, see below) and the engine plugins `DataValidation` and (since E12, the tag combo of the hit type) `GameplayTagsEditor`. Not `GSTNCombatDebug`: since E14 the debug plugin registers its own
  editor tab, so the two plugins have no edge in either direction.
- `GSTNCombatEditor` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GSTNCombatCore; private ApplicationCore (clipboard), AdvancedPreviewScene and AnimGraph (the preview scene and the anim instance montages play on, E6),
  AssetDefinition (`UAssetDefinition`; `UAssetDefinitionDefault` itself is in UnrealEd), DeveloperSettings, EditorFramework, InputCore,
  PropertyEditor (the Details tab, the timeline data row and the hit data row), GameplayTagsEditor (`SGameplayTagCombo`), WorkspaceMenuStructure (the validation tab's menu group), Slate, SlateCore, UnrealEd (the toolkit, `FBlueprintEditorUtils`, `FKismetEditorUtilities`), ContentBrowser and AssetTools (the save dialog of Extract), GSTNCombatGAS, GSTNCombatNP, GSTNInputCore, and for validation DataValidation
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
| `DefaultPreviewKit`, `DefaultPreviewSkin` | none | The player subject the preview starts with when an asset has none of its own (E6). The subject of an asset is stored per user, see the preview section |
| `bBakeOnSave` | off | Bake sockets when a timeline is saved (E8); baking is a button otherwise |
| `bAutoKeyDefault` | on | A gizmo drag on a frame without a key creates the key first (E7) |
| `TrackColors`, `GenericTrackColor` | one per M3 track kind | Row colours by track struct name |
| `FallbackTickRateHz` | 62.5 | Used only when the Network Prediction settings object cannot be read |
| `MirrorNamePairs`, `MirrorForwardAxis` | `_l/_r`, `Left/Right`, `.L/.R`; +X | Hurtbox mirroring (E9) |
| `SkeletonMirrorRules` | empty (every skeleton Auto, global pairs) | Per skeleton: switch mirroring `Off`, or give it its own name pairs (E9) |
| `HurtboxSaneRadiusCm` | 600 | Farther than this from the root a hurtbox is reported as probably misplaced (E9) |
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
validate, bake): `GetTickRateHz`, `BakeTimeline`, `IsBakeStale` (E8).

## Replacing content

The plugin ships no content and references none by path. The preview kit, skin and mesh are soft references in the settings
(and, from E6, editor-only soft properties on the edited asset). A game picks its own in `DefaultGSTNCombatEditor.ini`.

## Tests

`Automation RunTests GSTNCombat.Editor`: `Preview.*` (E6, on the engine's own `SkeletalCube` and `BasicShapes` and a montage built in memory, so no project content): `PoseAtFrameMatchesSectionTime`, `EvaluateIsRepeatable`, `FollowRootFramesMesh`, `NonHumanoidSubjectHasNoBoneAssumptions` (placement and capsule for odd shapes, every bone reachable by its own name, no bone name in a literal of the preview sources), `AttachmentsHangFromSocketsWithFullTransform`, `KitSkinSubjectResolvesReadOnly`, `SubjectPersistsPerAsset` (a restart is `ReloadFromDisk`), `ControllerFollowsPlayheadAndMontage`, `OverlayRegistryDrawsRegistered`, and `Preview.Spike.MontageSampling` (S1, needs any project montage with a preview mesh, skips without). The snapshots `GSTN.Visual.Timeline.Preview` (viewport with a real montage and a sword, and the reference pose seen from the side) and `PreviewToolbar` need a renderer (`-RenderOffscreen`). `UiFixes.*` (UI test fixes: `PlayheadWorksWithoutLength`, `TotalFramesAndFafAreUndoable`, `EmptySpaceClickClearsSelection`, `TextIsReadableOnEveryBar`, `FindingsStoreAndList`, `GoToFindingSelectsAndMoves`, `LengthFieldsConstruct`), `Module.LoadsAndRegisters`, `Debug.NoDependencyOnDebugModule` (E14: no debug module, plugin or include in this plugin), `Settings.DefaultsFromPluginIni`,
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

- 0.13.0 (2026-10-25, E9): the hurtbox set editor (viewport, list, details, findings), the Physics Asset generator with regeneration modes, the mirror with per-skeleton rules, `Hurtbox.BoneMissing` and `Hurtbox.Outside`, `PreviewMesh` on the hurtbox set (editor-only, `GSTNCombatCore`); the preview controller can run without a timeline; tests `Hurtbox.*`.
- 0.12.0 (2026-10-25, E8): the socket baker, the staleness record and check, the Bake sockets button with its dot, Bake on save, `gstn.Combat.BakeAll`, `UGSTN_CombatEditorUtils`, the `BakedStale` hash rule; `FGSTN_BakeInfo` in `GSTNCombatCore`; tests `Bake.*`.
- 0.11.0 (2026-10-25, E12): the hit data row (groups, angle dial, tag combo, extension data, knockback preview); tests `HitData.*` and snapshot `GSTN.Visual.HitData.Row`; the plugin now depends on the engine plugin `GameplayTagsEditor`.
- 0.10.0 (2026-10-25, E5b): the Hitbox and HurtboxModifier track editors, the hitbox keyed adapter with one lane per hitbox, key gestures that keep lanes, `GetTrackColor`; tests `Model.Hitbox*`, `Model.AutoKeyCreatesInterpolatedKey`, `Registry.HitboxAndHurtboxRows`, snapshot `GSTN.Visual.Timeline.HitboxRows`.
- 0.9.0 (2026-10-25, E11): the M4 rules (hitbox keys and groups, hit data, variants, hurt states, baked sockets), the hurtbox set validator, `UGSTN_CombatValidatorBase::BuildReportForAsset`, Validate all (`gstn.Combat.ValidateAll`), the validation panel and its tab, the Frame Data tab that follows the timeline, lane markers; tests `Validate.*` (M4) and `GSTN.Visual.Validation.*`.
- 0.8.1 (2026-10-25, E3): the phase bar reads `FGSTN_TimelinePhases` of `GSTNCombatCore`.
- 0.8.0 (2026-10-23, E6): the preview scene: a generic subject (kit and skin, or any skeletal mesh) with attachments, stored per asset in the user's config; the pose set per frame by `FGSTN_PreviewPose`; camera presets, root capsule, facing arrow, overlay registry for M4; tests `Preview.*` and the snapshots `GSTN.Visual.Timeline.Preview*`.
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
