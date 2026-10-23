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

## E10 validation mechanism, M3 timeline rules, kit/weapon/preset validators

- plan E10 files: as planned, plus `Private/Validation/GSTN_ValidationReport.cpp` (the report's helpers), `GSTN_ValidationMessageLog.*` (the Message Log and tokenized-message side) and `GSTN_EditorValidators.*` (the `UEditorValidatorBase` children and their shared base). `FGSTN_ValidationReport` is `Public/Validation/`; the rules (`FGSTN_CombatValidators`, static functions) are Private. `FGSTN_ValidationFinding` has one field more than combat-m5-editor 9.1: `Detail` (text shown by the Show action; the slot findings put the input patterns there). `EGSTN_Severity` is declared Error, Warning, Info (not a UENUM).
- combat-m5-editor 12 `Validate(Track, Data, FGSTN_ValidationReport&)`: added to `IGSTN_TrackEditor` with an empty default, as E5 left it. The validation calls it once per track whose struct has a track editor (a derived struct uses its base's editor, like the rows) and fills the track index and first frame of every finding that has none.
- combat-m5-editor 9.1 / plan E10, wrap versus duplicate: `FGSTN_TimelineValidation` (Core) returns only `{bError, Text}` with no code. The editor classifies its messages into codes by their wording (`ClassifyCoreMessage`; test `CoreMessagesClassified` fails when Core rewords one) and parses "Track N" for the location. Core was not touched. The clean fix is a `Code` field on `FGSTN_TimelineMessage` set in Core's `Add` calls; recommended for the docs commit or the next M3 change.
- combat-m5-editor 9.2 codes the Core checks have no row for get codes of their own: `Timeline.PartSelector` (empty selector, unknown part or group), `Timeline.EmptyTrack`, `Timeline.Ricochet`; anything unclassified is `Timeline.Core`. The tail checks of Core (tail past the end, before the FAF, blend-out longer than the tail) are `FrameRange`. Core's "track ends after TotalFrames" is a warning, the spec table says `EndFrame < TotalFrames` as an error: Core's severity is kept, since a half-open window may end exactly at `TotalFrames` and Core is the runtime's rule.
- 9.2 `OverlapSameKind`: `Movement` x `Movement` (error) as written. `HurtboxModifier` (error): two overlapping tracks of different `Kind` whose selectors can address the same hurtbox (same part name, same group name, or part tags where one matches the other); this is conservative. `Tags` (warning): the spec says "contradictory lock sets", but there is no unlock tag, so the only disagreement two lock sets can have is a window owning the global lock `Input.Lock.Command` next to one with a narrower `Input.Lock.Command.<root>`, which the global one already includes; that is what is reported. Open point for the owner if "contradictory" meant something else.
- 9.2 `RootMotionMix`: "without an explicit MixMode" cannot be seen (`MixMode` always has a value, default `Override`). Reported when the montage has root motion and a `Movement` track has `MixMode == Override`; `Additive` counts as a decision. The montage is loaded by the validator (`LoadSynchronous`) and by the Validate button; the pure rule reads `FGSTN_TimelineRuleContext::bMontageHasRootMotion`.
- 9.2 `CancelWindowTarget`: the "matches no command of any kit that references the ability" part needs a scan of the kits. The rule takes the commands as an optional pointer in the context and checks them when given; no validator fills it yet (a scan per save would load every kit), so Data Validation does not run that part. E11's Validate All is the place to fill it. `OnHit`/`OnBlock` with no hitbox and the empty query are checked.
- 9.2 `MontageLength`: Core's `UGSTN_CombatTimeline::IsDataValid` checks the montage at 60 Hz (the NP rate is not visible to Core) and only when the montage happens to be loaded; the timeline validator checks it at the project's tick rate with the montage loaded. In Data Validation a timeline can therefore carry two montage warnings when the rates differ (messages differ in their numbers, so they are not de-duplicated). Core could take the rate as a parameter; not changed.
- 9.1 two surfaces, Data Validation order: `ValidateObjectInternal` calls the asset's own `IsDataValid` first and does **not** run the validators when it returns Invalid. So an asset that Core already rejects shows Core's messages only, the editor-only rules appear once Core's are fixed. A finding whose text the context already has (Core's, the kit's own) is not added again (`SkipsWhatIsDataValidSaid`).
- 9.2 kit/weapon/preset: no new logic. Kit and weapon: their own `Validate` (already run by their `IsDataValid`) are findings `Kit.Data`/`Weapon.Data` for the toolkit and panel, and the validators add the command-table check. The table is `FGSTN_BindingTableCompiler::Compile` + `FindConflicts` (what the game runs), so only slots that some binding uses are compared; the spec's "compiled command set of each kit" is read this way (comparing unbound slots would flag conflicts the game's own loadout validation resolves with a binding's `PriorityOffset`). The slot findings are rebuilt per weapon context from the table to know which slots they are about (about 15 lines that mirror the context selection of the compiler's check); the compiler's own texts for them are replaced, the other table problems pass through as `Loadout.Table`.
- 9.2 "each weapon on top of the kit": the weapon validator finds the kits that can carry the weapon through the Asset Registry (`PrimaryWeapon`, or `AllowedAdditionalWeapons` matching the weapon's tags; an empty query allows none) and reports the findings that appear only with the weapon (the kit alone is the baseline). A kit that carries the weapon only through its query gets it checked as weapon 0, not as an additional one (the contexts are per weapon either way).
- 9.2 preset: **there is no `UGSTN_LoadoutDefinition` in the code** (M3 has the save-game presets of `UGSTN_LoadoutSubsystem` and `FGSTN_LoadoutData`; the designer asset is only in the docs). `UGSTN_LoadoutPresetValidator` therefore validates any asset whose class has an `FGSTN_LoadoutData` property (reflection), which includes that class when it is added. A conflict on a preset names the weapon or super slot it comes from ("comes from weapon 0 (Name)", "comes from super slot 1 (Name)"); the compile's other findings are passed through and the loadout rules' texts that repeat them are dropped.
- 9.2 notation: the input plugin has no formatter for a pattern, only `FGSTN_InputScript::Describe(History)`. The Show detail plays the slot's canonical input (`FGSTN_InputPattern::AppendCanonicalInput`, the same input the slot validator plays) into a history and describes that, with frames re-based to the first event and the button letters explained (`A=Input.Button.Attack`; letters follow the sorted tags of the two slots). A formatter in the input plugin would be cleaner; not added there.
- 9.1 `UGSTN_AbilityTimelineValidator`: **not declared**. M3 has no ability class with an inline timeline (`UGSTN_GA_BaseCombat` is M3-C10, not in this branch). E13 adds it (see the E13 section below).
- Show action: a `FActionToken` on the Message Log / Data Validation message that opens a dialog with the detail. The Data Validation path uses `FDataValidationContext::AddMessage`, so the entry has the asset link and the token; the Content Browser validation badge follows from the validator result. Not seen in a rendering editor.
- Validate button and F7: `FGSTN_TimelineEditorToolkit::ValidateNow` runs the timeline rules on the model's data at the model's tick rate with the montage loaded, writes a page named "Validate <asset>" to the Message Log category `GSTNCombat` (registered by the module, label "GSTN Combat") and opens it. F7 is mapped on the toolkit's command list, not the panel's (the E4 test that the panel leaves Validate unmapped stays true). The Frame Data tab still shows its placeholder text; the continuous findings are E11.
- Build: `GSTNCombatEditor.Build.cs` gets `bUseUnity = false`. Adding the E10 files changed UBT's unity bundles and two older test files (`Model`, `Registry`) ended in one bundle, where their file-scope `using namespace` helpers (`FFixture`, `AppliedSteps`) are ambiguous. New private dependencies: DataValidation, MessageLog, AssetRegistry, Projects, GameplayAbilities, GSTNInputCore; `.uplugin` lists `GSTNInput` and `DataValidation`.
- Not verified headless (needs a rendering editor): the Validate button and F7 in the toolkit, the Message Log tab and its page, the Show dialog, the validation badge in the Content Browser, validation on save of a real asset. Checked headless: the four validators register with Data Validation (`-run=DataValidation -AssetType=GSTN_CombatTimeline` lists them; no project asset of that class exists), and their results through `ValidateLoadedAsset` in the tests.

## E13 inline mode (Blueprint timelines)

- combat-m5-editor 4.1 / 2.1 item 3: the inline branch of `FGSTN_TimelineEditContext` is real. Target = the Blueprint, owner = the default object of its generated class, resolved on every call (`GetDefaultObject(false)`, null while `bBeingCompiled`, when the class is not a `UGSTN_GA_BaseCombat` or has a newer version). The spec's `Blueprint` member is `GetBlueprint()`. New on the context beyond the spec: `FromOwnerObject` (what a details row resolves), `CanEditInline`, `IsInlineShadowedByAsset`. `Modify()` now also sets `RF_Transactional` on the owner (a default object made by a compile may not have it; the toolkit did this once at init, which a compile undoes). `NotifyChanged` inline sends `PostEditChangeProperty` for `Timeline` to the default object, then `FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint, Event)`; it does not recompile.
- Registry: `Acquire` now also refuses a context that does not resolve (before: only a null target). A Blueprint that is not an ability, or is mid-compile, claims nothing.
- Dependency: `GSTNCombatEditor` -> `GSTNCombatNP` (plan R-B exception (c)), for `UGSTN_GA_BaseCombat`. README dependency paragraph rewritten ("Not a dependency" no longer lists `GSTNCombatNP`; `NetworkPrediction` is still not named by the editor, but it is in the link graph through `GSTNCombatNP`). The `.uplugin` lists `GSTNCombatNP`. Build.cs adds `ContentBrowser` and `AssetTools` (save dialog and unique name of Extract).
- Door: the plan's "button in the Blueprint editor's details" is a property type customization (`FGSTN_TimelineDataCustomization`, registered for `FGSTN_CombatTimelineData`). It is also on the `Data` of a timeline asset; elsewhere it shows the line and no button. No toolbar or menu entry in the Blueprint editor: the spec asks for the details row only and for no custom Blueprint editor, and a toolbar hook would add a second door into a Blueprint editor this plugin otherwise does not touch.
- Toolkit in inline mode: registered with the asset editor subsystem under the Blueprint (so Save saves the Blueprint and a deleted/closed Blueprint closes it) but `IsPrimaryEditor()` returns false, because `UAssetEditorSubsystem::FindEditorForAsset` would otherwise focus the timeline window when the Blueprint is double-clicked, instead of opening the Blueprint editor. `MarkBlueprintAsModified` also calls `FindEditorForAsset` and casts the result to `FAssetEditorToolkit`, which is safe here (`IsBlueprintEditor()` is false).
- Compile: `UBlueprint::OnCompiled()` (`AddSP`, removed in `Release`). Verified headless that the context already resolves inside the broadcast. Refresh = `Model->HandleUndoRedo()`. Gone = notification + `CloseWindow(AssetUnloadingOrInvalid)` on the next core tick. The decision is `GSTNInlineTimeline::DecideAfterCompile`, a plain function, because the toolkit cannot be built without a Slate application.
- Extract to timeline asset (OQ Q2 fallback): `GSTNInlineTimeline::ExtractToAsset(Context, NewAsset)`, one transaction on the default object: copies `Timeline` into the asset, sets `TimelineAsset`, clears `Timeline`. The toolkit wraps it with the Content Browser save dialog (`IContentBrowserSingleton::CreateModalSaveAssetDialog`), creates the asset, `AssetCreated`, closes the inline window and opens the asset editor. The default path is the Blueprint's own package path (no literal `/Game` in plugin code). Extract leaves the new asset dirty and unsaved, like any new asset.
- The ability has both `Timeline` and `TimelineAsset`; `GetTimelineData()` returns the asset's data only when the asset is set **and loaded**, else the inline one. So "the asset wins" is true at run time after the grant preloads it; the strip and the validator say "the Timeline Asset plays" and treat the inline data as unused when an asset is named. Worth one line in combat.md when the docs commit happens.
- 9.1 `UGSTN_AbilityTimelineValidator` is declared. `CanValidateAsset` = a `UBlueprint` whose generated class derives from the ability (public static `IsAbilityBlueprint`, because `CanValidateAsset` is protected in `UEditorValidatorBase`). Same rules as the asset validator on the default object's `Timeline` (`FGSTN_CombatValidators::ValidateAbilityTimeline`, which calls `ValidateTimeline`); an empty inline timeline is the same `FrameRange` error as an empty asset. New code `Ability.InlineIgnored` (warning): a `TimelineAsset` is named and the inline timeline still holds a length, a track or a montage; the inline data is then not checked. Not in the spec's 9.2 table. A class that was never compiled has no default object and is `NotValidated`. A Blueprint asset that is not loaded is not validated cheaply (Data Validation loads it).
- Open point (not verified): the details panel propagates an edit of a parent's default value to child Blueprints whose value still equals the old one (`FPropertyNode::PropagatePropertyChange`, private to PropertyEditor). The model writes the default object directly and does not. A child gets the value when the parent is compiled and the child is recompiled; if the engine's reinstancing does not do that for an unmodified child value, the child keeps the old inline timeline. Check in a rendering editor with a parent and a child ability Blueprint.
- Open point: an undo made after a compile finds the transaction recorded against the old default object. The undo client matches the live owner, so the step is simply not matched; the same limitation exists for a Class Defaults edit. The test `UndoRestoresCdoValue` covers edit -> compile -> edit -> undo (works), not undo across a compile.
- Not verified headless (needs a rendering editor): the Open Timeline Editor row in Class Defaults (button visibility per context, summary line), opening from it, the strip, the notification and the close after a reparent, the second-open focus, Double-click on the Blueprint still opening the Blueprint editor, Save writing the Blueprint, and the Extract dialog end to end. Checked headless: the context, compile (including the broadcast), the registry, the edit/undo/dirty chain on a real compiled Blueprint, Extract, and the validator.

## E14 input display (`GSTNCombatDebug`)

- plan E14 / combat-m5-editor 3, 10.1 ("editor side: nomad tab in `GSTNCombatEditor` via `FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>`"): **replaced** (owner decision, closes the E1 open point). The debug module registers its own nomad tab *Window > GSTN Combat > Input Display* in editor builds (`#if WITH_EDITOR`: `FGlobalTabmanager::RegisterNomadTabSpawner`, group `GSTN Combat` added under `WorkspaceMenu::GetMenuStructure().GetStructureRoot()`, both removed in `ShutdownModule`; `WorkspaceMenuStructure` is added to its Build.cs only when `Target.bBuildEditor`). The editor plugin includes no debug header and has no module or plugin edge to it. With the debug plugin off the tab does not exist and the editor builds and runs unchanged. `IGSTN_CombatDebugModule` stays (`CreateInputDisplay()` now returns the widget) but the editor no longer uses it; its comment says so.
- Plan test `GSTNCombat.Editor.Debug.TabSkipsWhenModuleAbsent` is replaced by `GSTNCombat.Editor.Debug.NoDependencyOnDebugModule`: no debug module in the editor plugin's module list, no debug plugin in its descriptor, no quoted module name in its Build.cs, no `#include "GSTNCombatDebug..."` in its sources.
- `GSTNCombatEditor.uplugin` no longer lists `GSTNCombatDebug` as `Optional` (delta E1 said it did; the entry made the debug plugin a descriptor-level neighbour). Version 0.7, README and Build.cs comment updated. The `.uproject` is untouched: both plugins stay listed there.
- Window menu placement and the tab icon (`ClassIcon.UserDefinedStruct`, the one other engine tabs use) are not verified: no rendering editor in this run.
- plan E14 files: the widget header is `Private/Widgets/SGSTN_InputDisplay.h`, not `Public/` (nothing outside the module names the class: the interface returns `SWidget`). Layout, `GSTN_InputDisplayScript` (notation to history to resolved command), `GSTN_LocalInputSource` (reads the local pawn) and `Overlay/GSTN_InputDisplayOverlay` are private too. The UMG wrapper of the README is not done (no consumer yet).
- combat-m5-editor 10.1 "frame gap printed in M1 4 notation (`7:+A`)": the label of an event is `<frames since the previous event>:<item>` (`1:+A` for the sample `0:face=90 0:4t 6:6t! 7:+A`: events 4t, 5, 6t!, +A are 0, 5, 1, 1 apart). A separate `ToNotation()` prints the strip as a script with frames counted from the first event, which replays to the same strip. A tap is `*A`, not `+A -A`; a stick back to neutral is its own event (`5`).
- 10.1 "consumed presses greyed (command tag on hover)": a press the history marks consumed is grey, with its hold and release. The command named on hover is the first marker at most 16 frames after the press (`FGSTN_InputDisplayLayoutParams::MarkerReachFrames`); with no marker in reach the press is still grey and the hover says "used by a command". The history does not record which command consumed a press, so this attribution is a heuristic.
- 10.1 "`Input.Event.CommandExecuted` marker": the markers are **not** taken from `UGSTN_SimEventSubsystem`. That event is published on the authority only, so a predicting client would never see its own commands. The local source reads the request the sync state holds (`RequestFrame`, `RequestedBinding`, resolved with `FGSTN_ActivationContext::FromRequest` and the command table), once per frame it polls, and drops markers past the newest frame after a re-simulation. A request that appears and is replaced between two polls is missed (the display polls every frame). Debug draw and the E15 ring may still use the subsystem on the host.
- Const accessor: `UGSTN_CombatSimComponent::GetInputHistoryView() const` (returns `const FGSTN_InputHistory*`, null before the proxy has a state), inline in the header, no `.cpp` change, no sync state change. It is the only edit in `GSTNCombatNP`.
- Scripted history: `FGSTN_InputDisplayScript::Resolve(Notation, ButtonTags, Slots, Settings)` plays the notation (Parse + Advance, not `Build`, so a typo is a message under the box and not a log error), resolves with the real `FGSTN_SlotResolver` on the last frame (every command counts as executable), consumes the winner's events and adds a marker. The widget resolves against `UGSTN_StandardSlots::Build()` until something calls `SetScriptSlots`. **Not done: the slots of "the open kit"**: a kit is data the editor reads (`FGSTN_CommandSet::Compose`), and the editor has no edge to the debug plugin any more, so nothing hands the kit's composed set to the tab. A later change can add a settings entry or a console command that takes a kit asset path, or the editor toolkit can hold its own scripted view built on `SGSTN_InputDisplay` once there is a shared Core-level home for it. The impossible-sequence errors of `Advance` (pressing a held button) still log an error line in addition to the message.
- Overlay: `gstn.Combat.Debug.InputDisplay 1` adds the strip to the lower right corner of every PIE or game viewport (one per PIE window, so the host and each client show their own local player), hit-test invisible, checked five times a second by a core ticker. The editor tab follows the first PIE or game world that has a local player with a combat component; with several PIE windows it shows the first, the overlay is the way to see each client.
- Dependencies of `GSTNCombatDebug`: private GameplayTags, Projects (tests read the plugin's own sources), Slate, GSTNInputCore, GSTNInputNP (the command table), GSTNCombatCore, GSTNCombatNP (the component), plus WorkspaceMenuStructure in editor builds. Plugin descriptor: GSTNInput, GSTNInputNP, GSTNCombatNP added. Version 0.2.
- Build note (not E14): `Source/GASP_Giustino2/Private/WallMovement/Tests/GSTN_WallSurfaceFilterTests.cpp` (T5) declares `GSTNWallFilterTest::Flags` and has a file-scope `using namespace`; in a unity bundle with `GSTN_WallMovementDebug.cpp` the global-name hiding warning C4459 on `Flags` (an error in this project) breaks `GASP_Giustino2`. It stays hidden while the file is in the git working set (adaptive unity builds it alone) and shows up the first time the module recompiles after the file is committed. Any header change that `Source/` includes triggers it (the E14 accessor did). Renaming the constant (`FilterTestFlags`) fixes it; E14 built with that rename applied temporarily and then reverted, because `Source/` is out of scope here.
- Not verified (needs a rendering editor and a PIE session): painting of the strip, the tooltip, the overlay in PIE (listen host + clients), the tab in the Window menu, the script box in the tab.

## Review fixes (2026-10-23)

- `WeaponSwitch` vocabulary (review C.2): the editor now uses `SwapFrame`, the frame the player fires and the validation checks.
  `FGSTN_TimelineEditorModel::GetTrackSpan` returns `[SwapFrame, SwapFrame + 1)` for a weapon switch, so drawing, hit-testing (`HitTestTrack` used
  `StartFrame` directly), move clamping and box select agree. `MoveWindow` shifts `SwapFrame` and sets `StartFrame` and `EndFrame` to follow it
  (keeping the window width), which repairs a stale copy instead of carrying the gap along. New warning `WeaponSwitch.FrameMismatch` from the
  weapon switch track editor's `Validate` when `StartFrame != SwapFrame`. A single field in Core (dropping `StartFrame` for the switch) would be
  cleaner and is Core's decision.
- Extract (review C.3): `GSTNInlineTimeline::ExtractToAsset(Context, NewAsset, ConfirmReplace)` refuses (false, nothing changed, no undo step) when the
  ability already names a `TimelineAsset` unless the callback answers yes; `ExtractWouldReplaceAsset` tells the toolkit to ask. The toolkit shows a Yes/No dialog
  before the save dialog and drops the new asset if the extraction does not happen.
- `IGSTN_CombatDebugModule` is exported (`GSTNCOMBATDEBUG_API`) (review C.1). The plan's wording "does not link" is replaced: the exported
  `Get()`/`IsAvailable()` need the module as a dependency; `FModuleManager::GetModulePtr` by name needs only the include path.
- Left as they are: C.4 (undo after a compile, declared), C.5 and C.6 (unverified), C.7 (`ClassifyCoreMessage` reads Core's English text; a message
  code in Core is the structural fix and Core is another thread's), C.8 to C.12 (validator gaps, registry keys, widget lifetimes, history copy per frame,
  style items), and the `Test` configuration note (documented in the debug README, descriptor unchanged).
- README fixes: the Play row and the E3/E14 status line of the editor README, the `GameplayAbilities` dependency note, the smoke-steps sentence, and the debug README's
  claim about reaching the module without linking.

## Second review fixes (2026-10-23, C.8 and nits)

- 9.2 (review C.8): new code `Ability.TimelineAssetUnresolved` (error, not in the spec's table). When the ability names a `TimelineAsset` that does not
  resolve (missing, or not a `UGSTN_CombatTimeline`), `ValidateAbilityTimeline` reports it; the reference still wins over the inline timeline, so the
  ability would play none. `FGSTN_CombatValidators::IsTimelineAssetResolvable(Reference, bLoad)`: in memory resolves; else the asset registry must know the
  asset and its class must derive from the timeline class; with `bLoad` (the Data Validation path) the asset is also loaded. The existing warning
  `Ability.InlineIgnored` is unchanged and can accompany the error. The asset registry is queried through `IAssetRegistry::GetChecked()`.
- Details panel (nit): `UGSTN_TimelineSelectionProxy::Commit` sets a weapon switch's `StartFrame` to its `SwapFrame` when the edit changed `SwapFrame`
  (the same repair `MoveWindow` does); an edit of another field leaves `StartFrame` alone. A comment in `GSTN_TimelineEditorModel.h` that still said
  the weapon switch fires on `StartFrame` is corrected.
- Still open from section C: C.4 to C.7 and C.9 to C.12 as listed above.
