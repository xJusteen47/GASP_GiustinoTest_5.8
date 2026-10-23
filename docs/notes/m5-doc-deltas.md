# M5 doc deltas (for the E19 docs commit)

One line each: where, what differs from the spec, why. Checkpoints in `docs/architecture/plans/m5-editor-plan.md`.

## E1 plugin skeletons, settings, hurtbox tags

- plan 1 ("each checkpoint appends to `Saved/Notes/m5-doc-deltas.md`"): this file lives in `docs/notes/` instead (task decision), so another machine sees it.
- combat-m5-editor 3 table / 3b: `GSTNCombatEditor` has no `StartupModule` registrations and no dependency on `GSTNCombatNP`,
  `NetworkPrediction` or `GSTNInput` yet. The only thing it needs from the fork is the tick rate, which it reads by reflection
  (spike S2), so the plugin links nothing of the fork; the edges are added when a module needs a type. `GSTNCombatGAS` is a
  dependency from E1 because the settings map bone names to the `Combat.Hurtbox.*` tags that module declares.
- combat-m5-editor 3 / 3b ("`GSTNCombatDebug` in `DynamicallyLoadedModuleNames`"): not done. UBT throws "Could not find
  definition for module" for a name in `DynamicallyLoadedModuleNames` that does not exist, which breaks the "editor builds
  without the debug plugin" requirement, and a module of an enabled plugin is built anyway. The editor will call
  `IGSTN_CombatDebugModule::Get()` (module-name lookup). Open point for E14: the editor needs the interface header; options are
  an include path added in Build.cs only when the sibling folder exists, or a tiny interface header in a shared Core module.
- combat-m5-editor 3 table (`GSTNCombatDebug` "compiled only when !bBuildEditor ? Configuration != Shipping"): UBT has no such
  rule on a module; the module is kept out of Shipping in the descriptor (`TargetConfigurationDenyList: ["Shipping"]`).
- `GSTNCombatEditor.uplugin` lists `GSTNCombatDebug` as `Optional`+`Enabled`, as the plan says. It builds with the debug plugin
  switched off in the `.uproject` (checked: build succeeds); an editor that enables the editor plugin also enables the debug
  one when present, so the two "disable independently" cases need the debug entry set to `false` in the project.
- `.uproject`: the two entries are appended after `GSTNSurface`; `GSTNCombatEditor` has `TargetAllowList: ["Editor"]`
  (editor only), `GSTNCombatDebug` has none (its descriptor already excludes Shipping). The file has LF line endings in the
  working tree (git converts: `core.autocrlf=true`), not CRLF as the task text said; tabs kept.
- combat-m5-editor 11: one `UDeveloperSettings` class for project-wide and per-user values; the split into a user-config
  class is deferred. Track colours are an array of `{TrackStruct, Color}` (an ini map of structs is fragile). Mirror forward
  axis is an enum of four axes. The Physics Asset mapping is `TArray<{BoneNameContains, Zone}>`, first match wins (E9 may refine
  it). `DebuggerKey` defaults to NumPad 5. Added `FallbackTickRateHz`.
- combat-m5-editor 7.1: `GetTickRateHz()` of `UGSTN_CombatEditorUtils` (E8) will wrap `UGSTN_CombatEditorSettings::GetSimulationTickRate()`,
  which exists from E1 (private header, same module) so E4's ruler can use it before E8.
- Spike S2 result in `m5-spike-report.md`: the property is `FNetworkPredictionSettings::FixedTickFrameRate`, reachable only by
  reflection because `UNetworkPredictionSettingsObject` has no export macro; the sim step is `(int32)(1000/rate)` ms, exact at 62.5 Hz.
- Tags: `Combat.Hurtbox.{Head,Torso,Arm,Leg,Hand,Foot}` added to `GSTN_CombatTags.h/.cpp` and to `GetDeclaredTags`; no earlier
  declaration existed (M4 had not added `Head`). `tag-owners.json` (modularity 7.1 step 5) does not exist in the repo yet, so no row added.

## E2 edit context, model, undo

- combat-m5-editor 4.3 and plan E2 ("a cancelled drag with Escape calls `CancelTransaction`"): `GEditor->CancelTransaction` only
  drops the undo record and does not revert the data (`UTransBuffer::Cancel`). The drag helper keeps a copy of the timeline data at
  mouse-down and restores it on Escape after dropping the record. The spec text should say "restore and cancel".
- combat-m5-editor 4.3 ("a drag opens the transaction on mouse-down"): it opens at the first change inside the drag. A click that
  moves nothing, or a drag that only meets the clamp, would otherwise leave an empty step in the undo history.
- combat-m5-editor 4.2: `MoveWindow(Track, Delta)` and `ResizeWindow(Track, Edge, Frame)` as written; added `EGSTN_WindowEdge`. Event
  and weapon-switch tracks fire on `StartFrame` and ignore `EndFrame`, so they have no edges (`ResizeWindow` refuses) and are
  clamped to `[0, TotalFrames - 1]`. Moving a weapon-switch track also moves its `SwapFrame` (the switch keeps its own copy of the frame).
- combat-m5-editor 4.2: gestures clamp to `[0, TotalFrames]`; a timeline with `TotalFrames == 0` has nothing to clamp to and is
  clamped at 0 only. `SetTotalFrames` changes only the total (windows past a shortened end stay as written; the validator reports
  them). `ApplyTrackEdit` and `ApplyDataEdit` (new, for the details panel and any non-gesture edit) never clamp.
- combat-m5-editor 4.2: `AddKey`, `MoveKeys`, `DuplicateKeys`, `PasteKeys`, `SetKeyTransform` are implemented against
  `IGSTN_KeyedTrackAccess` (new, `Private/Timeline/GSTN_KeyedTrackAccess.h`), a per-track-struct adapter. Added `RemoveKeys` (the
  Delete key) and `CopyKeys`/`CopyKeysToText`. Key frames are absolute timeline frames in the adapter contract. `PasteKeys` takes the
  target track (`PasteKeys(Track, Frame)`) instead of `PasteKeys(Frame)`: the clipboard text names the track struct and the key
  struct, and a mismatch is refused. M4's hitbox track must register its adapter and convert if it stores frames relative to a window.
- Clipboard text format: `GSTNKeys1<TAB>TrackStruct<TAB>KeyStruct`, then one `RelativeFrame<TAB>ExportText` line per key. The version
  tag is checked and bad text pastes nothing. Only keys of one track are copied (the first key's track). Plan Q4 (keys only) holds.
- combat-m5-editor 4.1: `FGSTN_TimelineEditContext` is `Public/Timeline/GSTN_TimelineEditContext.h` as planned; its inline branch is a
  stub (`ForBlueprint` never resolves; E13 fills it). The spec's `Owner` is split in `GetOwner()` (recorded in transactions, matched
  by undo clients) and `GetTarget()` (the asset or the Blueprint, the key of the one-toolkit-per-target registry).
- plan E2 ("`FEditorUndoClient` base in the model owner"): `FGSTN_TimelineUndoClient` is a small class the owner creates next to the
  model, not a base class of the model, so the model stays a plain object and the toolkit composes it.
- combat-m5-editor 4.2 lists `PreviewKitSkin` on the model: not added in E2 (the preview arrives in E6).
- combat-m5-editor 4.2 `FitTotalFramesToMontage`: uses the validator's formula (section length, or the whole montage, over the play
  rate, times the tick rate), rounded to the nearest frame, and loads the montage synchronously because the command asked for its
  length. The tick rate is the model's `TickRate`, from `UGSTN_CombatEditorSettings::GetSimulationTickRate()` (S2).
- Tests: the model tests are `EditorContext` only (they need `GEditor->Trans`). `ClipboardRoundTrip` tests the text functions, not the
  OS clipboard (`PasteKeys` and `CopyKeys` are thin wrappers), so it stays deterministic on a build machine. Test-only types live in
  `Private/Tests/GSTN_EditorTestTypes.h` (a hidden keyed track struct and its adapter). The extra test `ViewTransformRoundTrip` covers
  `FGSTN_TimelineViewTransform`, which the plan names in E2 and E4 tests in E4 will reuse.

## E4 timeline panel

- plan E4 files: the widgets are in `Private/Timeline/Widgets/` as planned. Added, widget-free: `GSTN_TimelineLayout` (hit-testing, box
  select, ruler spacing, navigation, wheel, playback clock), `GSTN_TimelineGesture` (the mouse state machine and `FGSTN_TimelineActions`) and
  `GSTN_PhaseBarData`; plus `GSTN_TimelineWidgetCommon.h` (colours, drawing helpers) and `GSTN_TimelinePanelTest`. Build.cs gains `Slate`, `SlateCore`.
- plan E4 ("phase bar from E3"): E3 (`FGSTN_TimelinePhases`, Core) is not in. The panel draws from `FGSTN_PhaseBarData`, a private stand-in in the editor
  module with the same rules as combat-m5-editor 5.3 (merged hitbox windows, startup = frames before the first one, recovery up to the FAF,
  "No hitbox" form of the summary, tail from `Data.GetTailStartFrame()`). **When E3 lands, the phase bar and `SGSTN_FrameDataSummary` call
  `FGSTN_TimelinePhases::Compute` and `GSTN_PhaseBarData.*` is deleted.** Core was not touched.
- combat-m5-editor 5.2 ("window = bar from StartFrame to EndFrame inclusive"): `GSTN_TimelineTypes.h` says ranges are half open, and the E2 model
  clamps to `[0, TotalFrames]` on that basis, so the bar covers `[StartFrame, EndFrame)`: from the start of the first frame to the start of the frame
  after the last. A one-frame window is one cell wide. Event and weapon switch tracks (they ignore `EndFrame`) are drawn as a diamond, not a bar.
- combat-m5-editor 5.2 (playhead `Home`/`End` first/last frame) vs 6 (`Home` = FrameAll): followed 6. `Home` frames the whole timeline, `End` is not bound.
- combat-m5-editor 5.2 ("`Space` plays ... honouring Play range"): there is no play range until the toolbar of E5; playback loops the whole timeline
  `[0, TotalFrames)`, from real time through `FGSTN_PlaybackClock` (frames skip after a slow editor frame instead of the move slowing down; one call
  never accounts for more than 0.25 s). A `FTSTicker` advances the playhead and an active timer keeps Slate awake while it runs.
- combat-m5-editor 5.2 ("wheel scroll, Ctrl+wheel zoom"): the wheel over a lane, the ruler or the phase bar moves the view in time, so the rows scroll
  with their own scrollbar or with the wheel over the name column. The view is clamped to stay near the timeline (a quarter of the view before the
  start, three quarters past the end).
- combat-m5-editor 5.2 (name column "mute/solo-preview toggle and + for adding keys"): the mute/solo toggle is for the preview scene and arrives
  with E6. The "+" is there only on tracks that have keys (it adds a key at the playhead).
- combat-m5-editor 5.2 ("Ctrl+drag duplicates"): applies to keys (`DuplicateKeys`), as the clipboard rules (plan Q4: keys only). A window is a track here,
  so there is nothing to Ctrl+drag; the copies appear with the first move off the pressed frame and are what the mouse then drags (one undo step).
- combat-m5-editor 5.2 ("double-click adds a window with the default length from the track editor"): E5's track editors do not exist, so a double-click on
  empty lane space adds a key on a keyed track (not twice on a frame), and on any other track a new track of the same kind starting there, 6 frames long
  (`FGSTN_TimelineActions::DefaultWindowFrames`), one frame for event and weapon switch. E5 replaces the constant by the track editor default.
- combat-m5-editor 5.2 ("drags clamp to `[0, TotalFrames - 1]`"): same as the E2 delta, windows to `[0, TotalFrames]` (a window may end on `TotalFrames`),
  keys to `[0, TotalFrames - 1]`. Several selected windows dragged together share one clamp, so they keep their distances.
- A press and release without a move on a member of a multiple selection narrows the selection to it (usual list behaviour); a drag keeps the group.
- Phase bar: the FAF marker writes `FirstActionableFrame` (`SetFirstActionableFrame`, nearest frame boundary). The tail band is hatched and starts at
  `GetTailStartFrame()`; its handle writes `Tail.StartFrame` through the new `SetTailStartFrame`. A tail whose `StartFrame` is negative (follows the FAF)
  stays that way until the handle actually leaves its frame; after that it is explicit. The tail is drawn only when `bHasTail` is on, turning it on is a
  details-panel edit. Hovering the bar shows the frame data line.
- Model changes (editor module, E2 types): `IsPointTrack`, `GetTrackSpan`, `GetFrameLimit` are public statics (one definition of the frame vocabulary for the
  gestures and the lanes), `SetTailStartFrame`, and `OnViewChanged`/`NotifyViewChanged` (repaint only: scroll, zoom and the box-select rectangle).
- Commands: the set holds step, previous/next key, play, add key, auto-key, delete, copy/paste/duplicate, frame all/selection and fit-to-montage, which the panel
  binds, and Bake (Ctrl+B), Validate (F7) and the three view toggles, which are declared so the chords are reserved and appear in Keyboard Shortcuts, and are
  mapped by the toolkits of E6, E8 and E10. The translate/rotate/scale chords (W/E/R) of combat-m5-editor 6 belong to the viewport client (E7) and are not in this set.
  `FUICommandList::CanExecuteAction` is true for an unmapped command, so the toolbar shows only mapped ones.
- Rows are an `SListView` of `FGSTN_TimelineRowData`, rebuilt only when a track appears, disappears or changes struct (add, delete, undo, redo); moves and
  resizes repaint. The keyboard focus goes to the panel when a lane, the ruler or the phase bar is clicked (`FReply::SetUserFocus`), and the panel runs the command
  list on key down. Escape cancels the drag and releases the mouse capture.
- Test tab: `gstn.Combat.OpenTimelinePanelTest [stress]`, editor only, a nomad tab with a hidden menu entry. It borrows the hidden keyed track and its adapter from
  `Private/Tests/GSTN_EditorTestTypes.h` so diamonds can be tried before the hitbox track exists; E5 removes the tab, the command and that dependency. The footer
  shows the paint time of the last repaint (sum of the lanes, closed by the ruler) as the acceptance readout of "12 tracks x 60 keys".
- Plan E4 ("manual: `Tools/GASNPPTest/gstn_editor_check.py` smoke steps"): that script is outside this branch's remit (another machine works there); the smoke steps are
  listed in the commit message instead.
- Not verified headless (needs a rendering editor): painting (colours, hatching, diamonds, labels), the mouse and cursor handling in real windows, keyboard focus routing,
  tab docking, the paint time. The maths, the gestures, undo, the commands and widget construction are covered by `GSTNCombat.Editor.Timeline.*`.

## E5 toolkit, registry, details proxy, built-in track editors

- plan E5 files: as planned, plus `Private/Timeline/GSTN_TimelineTrackEditor.cpp` (the registry; the header is Public), `Widgets/SGSTN_TimelineDetails`,
  `SGSTN_MontageLengthBar` and `SGSTN_TimelineToolbarExtra`. `GSTN_TimelinePanelTest.*`, the console command `gstn.Combat.OpenTimelinePanelTest` and the test
  `PanelTestCommandRegistered` are deleted; `Private/Tests/GSTN_EditorTestTypes.h` is now included by tests only (it gained `FGSTN_TestDerivedMovement`).
  Build.cs gains `AssetDefinition`, `EditorFramework`, `PropertyEditor`. `UAssetDefinitionDefault` itself is in UnrealEd (`AssetDefinitionDefault.h`), `UAssetDefinition`
  in the engine module `AssetDefinition`; no plugin dependency is needed for either. The definition has no factory (timelines stay Data Assets of the class), it only
  changes how they open; category Gameplay > Combat.
- combat-m5-editor 12 `IGSTN_TrackEditor`, differences from the listing: `GetTrackStruct()` returns `const UScriptStruct*`. `GetColor()` has a default (the settings colour of
  the struct, so a designer re-colours without code) and so does `HasKeys()` (an `IGSTN_KeyedTrackAccess` is registered for the struct: one source of truth). Added
  `GetIconBrushName()` (an `FAppStyle` brush name, not a brush: the public header stays free of Slate), `GetDefaultLength()` (with `FallbackDefaultLength = 6`, replacing
  `FGSTN_TimelineActions::DefaultWindowFrames`, which is removed; a point track still takes one frame whatever the editor says) and `AsGizmoProvider()` (no RTTI).
  `InitNewTrack(Track, AddedAtFrame, Data)`: the second argument is the frame the track was added at (the spec said playhead), and **the model's `AddTrack` calls it**
  inside the add transaction, so every way of adding a track gets the same defaults. The weapon switch's `SwapFrame` default moved from the model into its track editor.
- combat-m5-editor 12 `Validate(Track, Data, FGSTN_ValidationReport&)`: **not declared yet**. `FGSTN_ValidationReport` is an E10 type; E10 adds the virtual with an empty default,
  which breaks no registered editor.
- `IGSTN_GizmoProvider` and `FGSTN_TrackPreviewContext` are declared as specified in 12 and plan E5 and nothing calls them before E6/E7. Open point for E7: `ApplyWidgetDelta`
  takes `FGSTN_TimelineEditorModel&`, whose header is Private to the module, so a plugin outside it can implement the query half of a gizmo provider but not the write half
  until E7 publishes the model's mutation surface (or a narrower write interface).
- Registry (`FGSTN_TrackEditorRegistry`): `Find` also walks the base structs, so a game's track derived from a built-in keeps the built-in's row until it registers its own.
  `FindOrGeneric` never fails: the generic editor (window bar, name from the struct, colour from the settings, details only) is made on demand and cached per struct.
  Add Track lists the registered editors, then an "Other track types" section for every concrete non-hidden track struct without an editor, so the declared Hitbox and
  HurtboxModifier shells can still be added (as generic rows) until E5b. `OnChanged` is broadcast on register and unregister (nothing listens yet).
- Built-in editors: CancelWindow, Movement, Tags, Event, WeaponSwitch. **There is no Charge track in M3's Core** (`GSTN_TimelineTypes.h` has none), so there is no charge editor;
  the plan's list names one. The CancelWindow "condition icon" is a text suffix on the bar ("[on hit]", "[on block]", "[on whiff]"); the registry carries one icon per row, not
  per state. The icons are `Icons.Next`, `Transform`, `Details`, `Event`, `Convert` (verified in `StarshipStyle.cpp`), picked by meaning and easy to change.
- Row wiring: the colour swatch, the bar colour, the bar text (`GetRowLabel`, clipped to the bar), the name column (icon, `GetDisplayName`, row label in the tooltip) and the
  double-click length all come from the track editor of the row's struct. A key on a keyed track is still found through the keyed access registry, not the track editor.
- combat-m5-editor 5.1 Details: the proxy `UGSTN_TimelineSelectionProxy` (transient UObject) owns a `TInstancedStruct<FGSTN_TimelineTrack>` **copy** of the selected track, and the
  Details tab shows it with `IStructureDetailsView` over an `FStructOnScope` that points into the copy; `OnFinishedChangingProperties` calls `Commit`, which writes the copy back
  with `ApplyTrackEdit`. The spec's "pointer to the track" is a copy on purpose: an add, delete or undo moves the memory a pointer would aim at. An interactive change (slider)
  opens the model's drag on the first change and the committed change closes it, so a slider drag is one undo step. The proxy follows its track **index**, like the selection:
  when a track is removed, the one that moves into the index is shown; when the index no longer exists the proxy lets go. The tab shows one row at a time (a selection across
  rows, or nothing, shows a hint). **Keys are not shown yet**: `IGSTN_KeyedTrackAccess` hands out only const key memory, so editing a key's fields in the panel needs a mutable
  accessor next to `SetKeyTransform`; that arrives with the hitbox adapter (E5b). Selecting a key shows its track.
- Toolbar (combat-m5-editor 6): the skin and kit combos (stubs, disabled), Validate (disabled; its label is the command's) and the loop range are added to the end of the
  **panel's** toolbar through a named slot (`SGSTN_TimelineToolbarExtra`), not to the asset editor's toolbar. The panel already holds play, step, frame field with milliseconds,
  auto-key, add key, **Add Track**, frame all/selection and Fit Length; one toolbar in one place avoids a second set of `UToolMenus` extensions. The asset editor's default
  toolbar remains, with Save. The panel's toolbar is a single row and clips at narrow widths; wrapping it is cosmetic work for a rendering editor.
- Loop range (E4 left "Play range" for the E5 toolbar): `FGSTN_TimelineEditorModel::SetPlayRange(Start, End)` / `ClearPlayRange` / `GetEffectivePlayRange`. Editor state like the
  view: not data, not undone, not saved, cut to `[0, TotalFrames]`, and the whole timeline when empty or left outside by a shorter timeline. Playback loops over it.
- Warning bar and Fit Total Frames: `SGSTN_MontageLengthBar` sits above the panel in the Timeline tab and shows only when the montage's length in frames differs from
  `TotalFrames`. It loads the montage once per path (as the Fit command does) and never changes `TotalFrames` itself; the button is `FitTotalFramesToMontage`, one undo step.
- Layout: `GSTN_TimelineEditor_Layout_v1` as combat-m5-editor 5.1: viewport above the timeline on the left (0.45 / 0.55), details above frame data on the right (0.65 / 0.35).
  The Viewport tab is a placeholder until E6; the Frame Data tab holds the summary line and a note for the findings that arrive with the validators. The editor saves and
  restores the layout by its name.
- One editor per asset: `InitEditor` claims the context before building anything; when another toolkit holds the target, that one is focused (`FocusWindow`) and the second is not
  opened (returns false). `UAssetEditorSubsystem` normally focuses an open editor before `OpenAssets` runs, so the lease is the backstop for paths that skip it. `InitEditor` sets
  `RF_Transactional` on the asset, as the engine's own editors do, because an asset made without the flag would record nothing and undo would have nothing to restore.
- Test notes: `TwoToolkitsOneAssetFocusesOpen` tests the claim the toolkit makes (second claim refused, first claimant focused, close frees the asset) with a stand-in callback.
  Building the toolkit needs a window; `-nullrhi` has no Slate application. `DetailsFollowsSelection` constructs the Details tab and the montage bar only when a Slate
  application exists (it does not in this run). The layout test reads the layout text; the asset definition test asks the registry for the class.
- Not verified headless (needs a rendering editor): opening a timeline from the Content Browser, the four tabs and the saved layout, the structure details view over the proxy
  (including that the tag query and instanced struct fields edit correctly in a struct-only details view), the Add Track menu and icons, the colours/labels/icons on the rows, the
  warning bar, the loop range controls, Save, the tab and window behaviour of a refused second toolkit, and all painting.
