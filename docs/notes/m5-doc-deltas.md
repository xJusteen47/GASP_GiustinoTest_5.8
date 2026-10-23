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
