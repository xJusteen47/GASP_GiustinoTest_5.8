# M5 implementation plan: editor tooling (`GSTNCombatEditor`, `GSTNCombatDebug`)

Plan only (docs). Checkpoints are named E0, E1, ...; M4 and M4b checkpoints (H0..Hn, P0..Pn) are written by another planner, so
this file states dependencies as the **capability needed + spec section**; the lead maps them to H/P ids (section 2).

## 0. Sources and spec coverage

| Id | Source |
|---|---|
| S | `docs/architecture/combat-m5-editor.md` (this milestone's spec; sections quoted as `S §n`) |
| OQ | `docs/architecture/open-questions-m4-m10.md`, M5 section (Q1..Q20; Italian) |
| DD | `docs/architecture/game-design-direction.md` (wins over specs; §1 parts, §6b listen server, §6c tails) |
| MT | `docs/architecture/modularity-and-tags.md` (layering §3, reuse §3.7, Blueprint §3.8, replaceable content §3.9, tags §4, extension §5b) |
| M3 / M4 / M4b | `combat-m3-moves-kits.md`, `combat-m4-hits.md`, `combat-parts.md` |
| T | `docs/architecture/testing.md` (harness) |

Checkpoint to spec map:

| E | Spec sections implemented |
|---|---|
| E0 | S §2.2 (spikes S1..S4) |
| E1 | S §3, §3b, §3c, §11 (module and plugin skeletons, settings, tags) |
| E2 | S §4 (edit context, model, undo) |
| E3 | S §5.3 (phases, Core) |
| E4 | S §5.2, §6 (timeline panel, commands, toolbar) |
| E5 | S §5.1, §5.2 (toolkit, built-in track editors), §12 (registry) |
| E6 | S §5.4 (preview scene) |
| E7 | S §5.5 (viewport client, gizmos, picking) |
| E8 | S §7 (tick rate, baker, staleness) |
| E9 | S §8 (hurtbox set editor, generator, mirror) |
| E10 | S §9.1, §9.2 M3-only rules, kit/weapon/preset validators |
| E11 | S §9.2 M4 rules, validation panel, Message Log, Bake all |
| E12 | S §5.1 (HitData customization, knockback preview) |
| E13 | S §2.1 item 3, §4.1 (inline mode) |
| E14 | S §3 (debug plugin skeleton), §10.1 (input display) |
| E15 | S §10.2, §10.2b (const accessors, Gameplay Debugger category) |
| E16 | S §10.3 (debug draw, BakeCheck) |
| E17 | S §10.4 (trace channel, sink) |
| E18 | S §10.4 M5.8 (Rewind Debugger, stretch, gated on S3) |
| E19 | S §12, §12b, §13, §14 (extension procedure, parts hooks audit, designer scenario, closing) |

## 1. Ground rules for every checkpoint

- **One checkpoint = one commit** (short imperative subject, reason in the body). Gate before each commit: full build (Rider or
  UBT with the editor closed for header/reflection changes, not Live Coding); `Automation RunTests GSTNCombat` (all prefixes:
  `Core`, `GAS`, `NP`, `Editor`, `Debug`) and `GSTNInput` green; M1..M4 tests and scenarios still green. Editor tests run with
  `-nullrhi` unless a test needs a viewport (E6, E7 tagged `NeedsRHI`, run manually in a rendering editor and listed in the commit).
- **Harness gate**: a checkpoint that touches NP, the sim component, hit code or anything compiled into the PIE game (E15, E16,
  E17, and E8/E11 where they touch Core data) runs the relevant scenarios at 60/15 ms with and without 2 % loss, **listen
  server (host + 2 clients) first, dedicated + 2 clients second**; pure editor checkpoints (E1..E13) gate on build + Automation
  only and say so. Expected: no new reconciles, sync state bytes unchanged (editor code changes none).
- **Epic coding standard**: `U/F/S/I/E/T` prefixes, `GSTN_` after the prefix per repo convention, `TSharedPtr`/`TSharedRef` for
  Slate and toolkits, `SLATE_BEGIN_ARGS`, `FText` with `LOCTEXT` (namespace `GSTNCombatEditor`) for every user-visible string,
  `check`/`ensure` only for programmer errors, `FScopedTransaction` for every mutation, `UE_LOG(LogGSTNCombatEditor)`, `TCommands`
  for commands, `FEditorUndoClient` for undo-aware widgets, `UDeveloperSettings` for settings, `UAssetDefinitionDefault` (5.x)
  instead of the legacy `FAssetTypeActions_Base`, `UEditorValidatorBase` for validation, `FAdvancedPreviewScene`,
  `FEditorViewportClient`, `FGameplayDebuggerCategory`. Verify every engine API name against the 5.8 source before use (S §15).
- **Module types**: `GSTNCombatEditor` is `Type: Editor` (plugin descriptor `Type: Editor`, `LoadingPhase: PostEngineInit`);
  anything compiled only for authoring but touching runtime data headers is `UncookedOnly` only if it must ship with `WITH_EDITOR`
  data (none planned: `FGSTN_BakeInfo` is `WITH_EDITORONLY_DATA` inside the Core struct, E8). `GSTNCombatDebug` is `Runtime`,
  excluded from Shipping (`bIsShippingExcluded` style rule in its Build.cs, S §3).
- **Native tags**: `UE_DEFINE_GAMEPLAY_TAG_COMMENT` with the who-adds/removes line, declared in the owning module. M5 declares
  only the `Combat.Hurtbox.*` zone tags (E1, in `GSTNCombatGAS` `GSTN_CombatTags.h`, reuse M4's `Head` if present) and nothing in the
  two new plugins. Roots native, leaves ini (MT §4.2b): the `Combat.Hurtbox.*` six are the designer-facing set; code never names
  a designer leaf of `Kit.`/`Weapon.`. Test tags only in `Config/Tags/GSTNCombatEditorTest.ini` of the project test content.
  **No tag in any sync/aux state** (M5 adds no sync state at all).
- **README per plugin** (`Plugins/GSTNCombatEditor/README.md`, `Plugins/GSTNCombatDebug/README.md`) is created in E1 and updated
  in the same checkpoint as every feature (module table, settings, console variables, extension steps). Docs under `docs/`
  (combat.md §3, §14, §17; roadmap) are edited only in E19 (one docs commit) unless the owner says otherwise; each checkpoint
  appends its doc delta and spike result to `Saved/Notes/m5-doc-deltas.md`.
- **Independence acceptance line "R-B" (every checkpoint)**: the new plugin code (a) builds and its tests run **without the
  project module** (`GASP_Giustino2`): no include of it, no `/Game/` reference in plugin C++ or plugin Config, no project ini key,
  no project tag; (b) settings live in the plugin's own `UDeveloperSettings` with defaults in
  `Plugins/<X>/Config/Default<X>.ini`; (c) depends on other plugins only through their `*Core` module (exception, documented in the
  README dependency paragraph: `GSTNCombatEditor`/`GSTNCombatDebug` -> `GSTNCombatNP`, the NP-of-same-layer exception already
  accepted for `GSTNCombatNP` -> `GSTNInputNP`; S §3b rule 3, OQ Q17); (d) optional integrations are bridge or hook APIs, never
  `#if WITH_<PLUGIN>`; (e) game tags are fields in settings or data; (f) tests carry the plugin prefix. Plus: **no runtime module
  depends on either new plugin** (grep of every `*.Build.cs` and `.uplugin`, in the gate of E1 and E19).
- **Blueprint extension (MT §3.8)**: the editor extension point is C++ (`IGSTN_TrackEditor`, `IGSTN_GizmoProvider`): editor
  tooling is not simulation, so §3.8's "one BlueprintNativeEvent" rule applies to the **data it edits** (unchanged), and to
  tuning structs the editor adds (`UGSTN_CombatEditorSettings` is `Config`, properties `EditAnywhere`). Documented in the README
  "Extending in Blueprint" paragraph: designers add **content** (timelines, hurtbox sets, validation severities in settings) not
  tools; Editor Utility Widgets can call the pure Blueprint-callable `UGSTN_CombatEditorUtils` (validate, bake, tick rate), added
  in E8/E11 as `BlueprintCallable, meta=(DevelopmentOnly)` static functions in the editor module.
- **Replaceable content (MT §3.9)**: no `ConstructorHelpers`, no literal `/Game/` path, no hard asset reference in plugin C++. The
  preview kit/skin/mesh are `TSoftObjectPtr` in editor-only properties and settings. Gate: grep for `ConstructorHelpers`,
  `"/Game/`, `TEXT("/Game`, `LoadObject<` with a literal path in `Plugins/GSTNCombatEditor` and `Plugins/GSTNCombatDebug`.
- **Network**: LISTEN server is the reference (DD §6b). The editor modules never run in a build that simulates. The debug module
  runs in PIE and development builds on the host and clients: it reads local data, never changes sync state, no `OnRep` use
  (the host never gets one), host-local events fire once (a ring fed by `UGSTN_SimEventSubsystem::Listen` must not also listen to
  a local duplicate; test in E15).

## 2. Dependencies on M3 and M4/M4b (what can start when)

### 2.1 Legend for dependency tags used below

- **M3-C2**: `FGSTN_CombatTimelineData`, `FGSTN_TimelineTrack` base and the declared track structs (CancelWindow, Movement, Tags,
  Event, WeaponSwitch, Hitbox and HurtboxModifier as declared shells), `UGSTN_CombatTimeline`, `FGSTN_TimelinePlayer`, timeline
  validation in `GSTN_TimelineValidation.h`, tail data (R-A). M3 spec §6.3.
- **M3-C3**: `FGSTN_HitVictimView`, extension-surface foundations (`FGSTN_KitExtension` base + `ExtensionData`), rules. MT §8.5.
- **M3-C5**: kit/weapon/super/skin/loadout **data** (`UGSTN_PrimaryKitDefinition`, `UGSTN_SkinDefinition` with the skin mesh,
  `UGSTN_HurtboxSetDefinition` pointer on the kit, `CompatibleSkins`). M3 §8.
- **M3-C6**: compiled binding table + `UGSTN_LoadoutRules` (validation sources for E10).
- **M3-C10**: `UGSTN_GA_BaseCombat` with the inline `Timeline` property; `PlaySyncedMontage` behaviour (blend-in, play rate,
  section start) needed to confirm the bake mapping.
- **M2**: `FGSTN_InputHistory` on the sim component and the notation/`FGSTN_InputScript` of M1 §4 (input display).
- **M4 capabilities** (to be mapped to H ids by the lead): **M4-shapes** (`FGSTN_HitShape`, `UGSTN_HitShapeUtils`, M4 §3);
  **M4-hitbox** (`FGSTN_HitboxTrack`, `FGSTN_HitboxSpec`, `FGSTN_HitboxKey`, `EGSTN_HitboxSpace`, evaluation function, M4 §3);
  **M4-hitdata** (`FGSTN_HitData` with its enums, `FGSTN_HitVariant`, `HitType` tags, M4 §3); **M4-hurtbox** (`FGSTN_Hurtbox` with
  `Priority`, `UGSTN_HurtboxSetDefinition`, `UGSTN_HurtboxModifier` track and `EGSTN_HurtState`, M4 §3, combat-parts §3.4);
  **M4-baked** (`BakedSockets` on `FGSTN_CombatTimelineData`, M4 §3 and §10); **M4-knockback** (`UGSTN_KnockbackModel` API and
  `FGSTN_ReactionTuning`, M4 §3 and §7); **M4-snapshot** (the world snapshot / hit and hurt shape query for debug draw, M4 §5);
  **M4-events** (`UGSTN_SimEventSubsystem` hit events with hit data id, relationship, M4 §5, §9b); **M4-hitstop-state** (hitstop
  and hitstun remaining as readable sim state, M4 §8 and §11); **M4-reaction** (victim reaction fields shown by the debugger:
  tier, stagger force, M4 §7).
- **M4b capabilities** (combat-parts): **P-hurtbox-priority** (`FGSTN_Hurtbox::Priority`, §3.4, also an M4 field);
  **P-hit-extension** (`FGSTN_HitData::ExtensionData` accepting `FGSTN_PartHitData`, §3, with M4 §3 `FGSTN_HitExtension`);
  **P-partset** (`UGSTN_PartSetDefinition`, §3) and **P-partmath** (`UGSTN_PartMath`, `UGSTN_PenetrationTable`, §8): only needed
  by `GSTNPartsEditor`, **not by any M5 checkpoint** (S §12b); M5 only provides hooks and a hooks test (E19).

### 2.2 Per-checkpoint dependency table

| E | Needs from M3 (done) | Needs from M4/M4b (capability) | Can start after M3 only? |
|---|---|---|---|
| E0 S2 (tick rate), S4 (gizmo spike, throwaway box) | none | none | **Yes, now** (needs editor session) |
| E0 S1 (montage sampling) | a test montage; the confirmation vs `PlaySyncedMontage` waits for C10 | none | **Yes** (result re-checked at E8) |
| E0 S3 (Insights/Rewind) | C8 (sync state exists), ideally C10 | none | after C8 |
| E1 | C1 (GAS tags header exists) | none | **Yes** |
| E2 | C2 | none (model is generic over `FGSTN_TimelineTrack`) | **Yes** |
| E3 | C2 (+ M4-hitbox for the active ranges; compute against the declared Hitbox shell, completes with M4-hitbox) | M4-hitbox (field `Hitboxes[].Keys/Window`) for the non-trivial tests | **Partly**: pure function + tests with synthetic hitbox tracks need only a minimal `FGSTN_HitboxTrack` shell; final tests wait for M4-hitbox |
| E4 | C2 | none | **Yes** |
| E5 | C2, C3 (generic fallback row) | M4-hitbox, M4-hurtbox for the Hitbox and HurtboxModifier track editors | **Yes for CancelWindow/Movement/Tags/Event/WeaponSwitch/Charge rows and the generic row**; Hitbox/HurtboxModifier rows wait |
| E6 | C5 (skin mesh), C2 (montage section) | M4-hurtbox + M4-hitbox + M4-shapes for the overlays; `FGSTN_ShapeLines` Core helper (added in E6 if M4 has not) | **Pose + mesh preview: yes**; box overlays need M4 |
| E7 | C2 | M4-hitbox (keys, spaces, evaluation fn) | No |
| E8 | C2; C10 for the final montage-start confirmation | M4-baked (`BakedSockets`), M4-hitbox (`Space`, `Socket`) | No (can start with a local shim of `BakedSockets` only if M4 is late: **not recommended**) |
| E9 | C5 (kit points at a hurtbox set) | M4-hurtbox, M4-shapes, P-hurtbox-priority | No |
| E10 | C2 (validation fns), C5, C6 (loadout rules), M1 slot validator | none for the M3 rules | **Yes** |
| E11 | E10 | M4-hitbox, M4-hitdata, M4-hurtbox, M4-baked | No |
| E12 | C2 | M4-hitdata, M4-knockback | No |
| E13 | C10 (`UGSTN_GA_BaseCombat` inline timeline) | none | **Yes** (after C10) |
| E14 | M2 (`FGSTN_InputHistory`), C6 (scripted history resolves in a kit) | none | **Yes** (needs M2 only; scripted lookup needs C6) |
| E15 | C8..C10 (sim state: timeline frame, action state, lock tags, adoption), C6 (registry names) | M4-hitstop-state, M4-events, M4-reaction (last-8-hits ring, hitstun/hitstop lines) | **Skeleton + M3 sections yes** (after C10); hit sections wait |
| E16 | C10 | M4-snapshot, M4-shapes, M4-hitbox, M4-hurtbox | No |
| E17 | C8..C10 (NP sim loop to hook) | M4-events (hit events in the trace) | **CombatFrame/state events yes** (after C10); hit/rollback events wait for M4 |
| E18 | E17 | none | gated on S3 |
| E19 | everything above | P-hit-extension, P-hurtbox-priority (hooks test with a fake extension struct; a real GSTNPartsEditor smoke only if M4b P-editor exists) | No |

**Parallel start with late M3 (summary)**: after C2 and C5 land, a thread can already run E1 -> E2 -> E4 -> E5 (non-hitbox) and
E10; after M2, E14. C10 unlocks E13 and the skeleton of E15/E17. Everything that draws or edits shapes, hit data, hurtboxes or
baked sockets (E3 final, E5 Hitbox rows, E6 overlays, E7, E8, E9, E11, E12, E16) waits for the M4 capabilities above.

## 3. Spikes first (E0)

Half a day each; results go to `Saved/Notes/m5-spike-report.md` (later combat.md §17; S §14 acceptance 1). Throwaway code lives
under `Saved/` or a scratch branch, never in a plugin.

### E0 Spikes S1..S4

| Spike | What to do | Pass | Stop and ask when |
|---|---|---|---|
| S1 Montage sampling (editor session, Python or a throwaway `UFUNCTION` in a scratch commandlet) | On a `UDebugSkelMeshComponent` with the mannequin and a montage section, set the position directly (no real-time tick), `TickAnimation(0)` + `RefreshBoneTransforms`, read `GetSocketTransform(Name, RTS_Component)` for frames 0..N, forward and backward, three runs; test with root motion on and off, with a blend-in, with a post-process anim instance (GASP) and an additive layer | Repeatable to 1e-4 cm across runs and equal backwards; extracted root motion absent from the evaluated component-space pose; notifies and blend-in do not move the pose; time per bake of 60 frames x 3 sockets recorded | The evaluated pose depends on a ticked post-process graph (cannot be set per frame) or includes root motion: then the bake needs a second code path (read the raw animation sequence with `GetBoneTransform` per frame) and the S §7.2 algorithm changes. Ask the owner before choosing raw-sequence baking, because it ignores layered anim instances. |
| S2 Tick rate source | In the fork, find the property holding the NP fixed tick rate (`UNetworkPredictionSettings` / `FixedTickFPS`), confirm 62.5 Hz and that it is readable with no world (CDO of the settings class) | One function returns 62.5 Hz in an editor with no world, equal to the runtime value logged in PIE | Only readable from a world manager: fall back to a plugin setting default `62.5` plus a warning in the validator when it differs from the runtime value; ask which source wins. |
| S3 Insights / Rewind | In PIE (listen, then dedicated) with the fork, does the Rewind Debugger record anything for NP pawns? Is there an NP Insights provider? Can a `UE_TRACE_LOG` event of ours show in Insights with a bookmark? | A written yes/no per question and the cost estimate for an analyzer | Rewind Debugger needs a TraceServices analyzer plus provider plus track creator (several hundred lines): E18 becomes optional, ask the owner before spending on it (OQ Q14). |
| S4 Gizmo in a custom viewport | One `FEditorViewportClient` with a `FWidget` on a virtual selection (a box that is not a component): `GetWidgetLocation`, `InputWidgetDelta`, hit proxy for picking, one `FScopedTransaction` per drag; undo | Translate/rotate/scale works, one drag is one undo step, picking selects the box, Escape cancels | Needs a dummy component per selectable key to get hit proxies: acceptable fallback (invisible `USceneComponent` per hitbox key in the preview scene only); no owner question unless it adds more than ~300 lines. |

Order: S2 (30 min, no UI), S1, S4, S3 (needs the NP sim running, after C8). **S1 and S2 results gate E8; S4 gates E7; S3 gates
E17 scope and E18.** If S1 shows the preview pose cannot be set per frame, E6 and E8 both change; stop and ask before E6.

## 4. Checkpoints (in order)

Legend: [plugin/module] - [files] - [tags] - [tests] - [acceptance] - [spec] - [size]. Every acceptance also carries the R-B line
of section 1. Paths are relative to `Plugins/<plugin>/Source/<module>/` unless written in full.

### E1 Plugin and module skeletons, settings, hurtbox zone tags

- **Plugins**: new `Plugins/GSTNCombatEditor/` (`Type: Editor`, deps: plugin `GSTNCombat`, `GSTNCombatNP`, `GSTNInput`, optional
  `GSTNCombatDebug` with `"Optional": true`, `GameplayTagsEditor`/`DataValidation`/`StructUtils`-equivalents as engine plugins as
  needed) and `Plugins/GSTNCombatDebug/` (`Runtime`, no Shipping, `Optional` flag for the GameplayDebugger dependency).
  No other plugin or project `.uplugin` references them; the project `.uproject` lists both (the one project edit, announced in
  section 6).
- **Files**: `GSTNCombatEditor.Build.cs` (S §3 deps; `GSTNCombatDebug` in `DynamicallyLoadedModuleNames`),
  `Public/GSTNCombatEditorModule.h`, `Private/GSTNCombatEditorModule.cpp` (empty registry hooks for later checkpoints),
  `Private/Settings/GSTN_CombatEditorSettings.h/.cpp` (S §11: default preview kit/skin as `TSoftObjectPtr`, `bBakeOnSave`,
  `bAutoKeyDefault = true`, track colours, mirror naming pairs and forward axis, Physics Asset to `Combat.Hurtbox.*` mapping as
  `FGameplayTag` fields, debugger key), `Config/DefaultGSTNCombatEditor.ini`, `README.md`; debug side `GSTNCombatDebug.Build.cs`,
  `Public/GSTNCombatDebugModule.h` with `IGSTN_CombatDebugModule` (empty `CreateInputDisplay()` declared, implemented in E14),
  `README.md`.
- **Tags**: `Combat.Hurtbox.{Head,Torso,Arm,Leg,Hand,Foot}` in `GSTNCombatGAS` (`GSTN_CombatTags.h/.cpp`) with
  `UE_DEFINE_GAMEPLAY_TAG_COMMENT` ("classification of a hurtbox; never granted to an actor"). If M4 already declared `Head`, do
  not redeclare.
- **Tests**: `GSTNCombat.Editor.Module.LoadsAndRegisters` (module loads in an editor with no combat content),
  `GSTNCombat.Editor.Settings.DefaultsFromPluginIni`, `GSTNCombat.GAS.Tags.HurtboxZonesRegistered`.
- **Acceptance**: both plugins enable/disable independently (build matrix: project builds with `GSTNCombatEditor` disabled, with
  `GSTNCombatDebug` disabled, with both disabled, with both enabled; S §3b); the editor plugin builds when `GSTNCombatDebug` is
  absent; a Shipping target build contains neither plugin; grep gate: no `*.Build.cs` of `GSTNInput*`/`GSTNCombat*` runtime modules
  names the new modules; tags visible in the picker.
- **Spec**: S §3, §3b, §3c, §11. **Size**: S.

### E2 Edit context, model, undo (pure, no Slate yet)

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Public/Timeline/GSTN_TimelineEditContext.h` (+ `Private/Timeline/GSTN_TimelineEditContext.cpp`):
  `FGSTN_TimelineEditContext` with asset and inline resolution (inline resolved by a stub until E13); `Private/Timeline/
  GSTN_TimelineEditorModel.h/.cpp`: `FGSTN_TimelineEditorModel`, `FGSTN_Selection`, `EGSTN_SelectionPart`,
  `FGSTN_TimelineViewTransform`, delegates `OnDataChanged/OnSelectionChanged/OnPlayheadChanged/OnKeySelected`; mutations of
  S §4.2 that exist on M3 types now: `AddTrack(UScriptStruct*)`, `RemoveTracks`, `MoveWindow`, `ResizeWindow`,
  `SetTotalFrames`, `SetFirstActionableFrame`, `FitTotalFramesToMontage`, `ApplyTrackEdit`, plus selection/playhead; the key
  mutations (`AddKey`, `MoveKeys`, `DuplicateKeys`, `PasteKeys`, `SetKeyTransform`) are declared and implemented against a
  small `IGSTN_KeyedTrackAccess` adapter registered per track struct so they land with the Hitbox track editor in E5/E7;
  `Private/Timeline/GSTN_TimelineUndo.*` (transaction helper for drags: begin on mouse-down, end on mouse-up, `CancelTransaction`
  on Escape).
- **Undo**: `FEditorUndoClient` base in the model owner (the toolkit in E5; here a test shell); `MatchesContext` true for the owner
  object; `PostUndo/PostRedo` revalidate selection and clamp the playhead. Key copy/paste uses `ExportText` of the key struct via
  `FPlatformApplicationMisc::ClipboardCopy` (per OQ Q4: keys only).
- **Tags**: none.
- **Tests** (`GSTNCombat.Editor.Model.*`, `EditorContext | ProductFilter`, transient `UGSTN_CombatTimeline`):
  `EachMutationOnlyChangesWhatItSays`, `ClampsToTotalFrames`, `UndoRedoRestoresExactData` (every mutation, `GEditor->UndoTransaction`),
  `DragIsOneUndoStep`, `CancelledDragLeavesNoStep`, `SelectionRevalidatedAfterUndo`, `ClipboardRoundTrip` (text), `FitTotalFramesDoesNotRunSilently`
  (data unchanged until the command), `TwoToolkitsOneTargetRefused` (the context registry).
- **Acceptance**: the model compiles with no Slate dependency (grep for `SWidget`/`Slate` includes in `GSTN_TimelineEditorModel.*`
  = none); widgets and gizmos will call only the model.
- **Spec**: S §4. **Size**: M. **Needs**: M3-C2 only.

### E3 Derived frame data in Core (`FGSTN_TimelinePhases`)

- **Plugin/module**: `GSTNCombat` -> `GSTNCombatCore` (this is a **runtime plugin change**, done here because Core owns pure frame
  data; if M3/M4 already added an equivalent, reuse it and this checkpoint shrinks to the summary string and tests).
- **Files**: `GSTNCombatCore/Public/Timeline/GSTN_TimelinePhases.h`, `Private/Timeline/GSTN_TimelinePhases.cpp`:
  `FGSTN_TimelinePhases::Compute(const FGSTN_CombatTimelineData&)` → `{ StartupEnd, ActiveStart, ActiveEnd, RecoveryEnd (= FAF),
  Total, ActiveRanges (merged), FirstHitFrame1Based }` (S §5.3); `FormatSummary` producing `Startup 7 | Active 3 | Recovery 14 |
  FAF 24 | Total 30 (484 ms)` with the tick rate passed in (never read from a world; OQ Q3 assumption: always visible, read-only,
  first active frame = index + 1). Hitbox windows muted by a relationship variant are ignored when the variant data exists
  (M4-hitbox); before that every hitbox window counts.
- **Tags**: none. **Tests** (`GSTNCombat.Core.TimelinePhases.*`): `NoHitbox`, `OneWindow`, `TwoDisjoint`, `OverlappingMerged`,
  `HitboxAtFrameZero`, `FafEqualsTotal`, `TailStartIsNotRecovery` (R-A tail is shown separately, not a phase), `SummaryString`,
  `NoAllocationBeyondOutput`.
- **Acceptance**: Core still has zero include of GAS/NP/Mover/editor headers (grep gate); runtime debug draw and harness may use it.
- **Spec**: S §5.3. **Size**: S. **Needs**: M3-C2; final tests need M4-hitbox (`FGSTN_HitboxTrack`), earlier tests use the declared shell.

### E4 Timeline panel (Slate): ruler, phase bar, rows, interaction

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Private/Timeline/Widgets/SGSTN_TimelinePanel.*`, `SGSTN_TimelineRuler.*`, `SGSTN_TimelineTrackRow.*`,
  `SGSTN_TimelineLane.*`, `SGSTN_PhaseBar.*`, `SGSTN_FrameDataSummary.*`; `Private/Timeline/GSTN_TimelineEditorCommands.*`
  (`TCommands<>` of S §6: step, play/pause, prev/next key, add key, auto-key, delete, copy/paste/duplicate, frame all/selection,
  bake, validate, fit total frames, view toggles; all rebindable).
- **Behaviour** (S §5.2): shared `FGSTN_TimelineViewTransform` (wheel scroll, Ctrl+wheel zoom around the cursor, `Home` frame all,
  `F` frame selection); ruler with frame ticks adapting to zoom plus a millisecond line (`frame x 1000 / TickRate`, tick rate from
  the settings function of E8 or, until then, a model field defaulted from `UNetworkPredictionSettings` per S2); FAF/Total
  markers; playhead red line, `,` `.` stepping, `Shift+,` `Shift+.` to previous/next key or edge, `Space` plays via `FTSTicker`
  at the real tick rate; lane painted in `OnPaint` (no child widget per key, 40 keys x 12 tracks cheap); click select, drag body
  to move, drag ends to resize (cursor change), `Ctrl+drag` duplicate, box select, `Delete`, `Ctrl+C/V`, double-click adds a
  window with the track editor's default length; integer frames only, drags clamp to `[0, TotalFrames-1]`; invalid windows
  hatched red; each gesture is one transaction via the E2 helper; phase bar from E3 with FAF marker draggable (writes
  `FirstActionableFrame` through the model); the **cancellable tail** (M3-C2 tail data, DD §6c) drawn as a hatched band after the
  FAF, draggable.
- **Tags**: none. **Tests**: `GSTNCombat.Editor.Timeline.ViewTransformRoundTrip` (frame<->pixel at several zooms),
  `...RulerTickSpacing` (label density), `...HitTestWindowEdges`, `...BoxSelect` (widget-free hit-test functions in the model;
  widget code kept thin); manual: `Tools/GASNPPTest/gstn_editor_check.py` smoke steps listed in the commit.
- **Acceptance**: panel hosted in a test nomad tab opened by a console command (`gstn.Combat.OpenTimelinePanelTest`, editor only,
  removed in E5) edits a transient timeline with generic rows; undo/redo works for every gesture; usable at 12 tracks x 60 keys
  without frame-time spikes (record the paint time).
- **Spec**: S §5.2, §6. **Size**: L (the largest Slate piece; S §15 risk 1; a usable cut at the end of E4+E5 with the details
  panel fallback). **Needs**: M3-C2 only.

### E5 Toolkit, registry, details proxy, built-in track editors

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Public/Timeline/GSTN_TimelineTrackEditor.h` (`IGSTN_TrackEditor`, `IGSTN_GizmoProvider`, `FGSTN_TrackEditorRegistry`,
  `FGSTN_TrackPreviewContext`, per S §12), `Private/Timeline/GSTN_TimelineEditorToolkit.*` (`FAssetEditorToolkit` +
  `FEditorUndoClient`; `InitEditor(Mode, InitToolkitHost, Context)`; standalone layout `GSTN_TimelineEditor_Layout_v1` with tabs
  Viewport, Timeline, Details, Frame Data; `GetObjectsCurrentlyBeingEdited`), `Private/Assets/GSTN_AssetDefinition_CombatTimeline.*`
  (`UAssetDefinitionDefault`: colour, open action, category), `Private/Timeline/GSTN_TimelineSelectionProxy.*`
  (`UGSTN_TimelineSelectionProxy`, transient, shown through `IStructureDetailsView`; edits via the model's `ApplyTrackEdit`),
  `Private/Timeline/GSTN_TrackEditors.cpp` (built-ins). Toolbar of S §6 (skin/kit combos stubbed, play/step, frame field with ms,
  loop range, Add track from the registry, Validate disabled until E10). Warning bar for montage length mismatch with the
  undoable **Fit Total Frames to montage** (OQ Q9: never automatic).
- **Built-in track editors in this checkpoint (M3-only)**: CancelWindow (label = command tags of the query, condition icon),
  Movement, Tags, Event (diamond), Charge, WeaponSwitch, and the **generic fallback row** (any unknown `FGSTN_TimelineTrack` child:
  window bar, name from the struct, details only; never breaks the editor). **Deferred to E5b (needs M4)**: the Hitbox row (one
  lane per `FGSTN_HitboxSpec`, diamonds for keys, label from `HitGroup` and `Space`) and the HurtboxModifier row (colour by
  `EGSTN_HurtState`).
- **Tags**: none. **Tests**: `GSTNCombat.Editor.Registry.RegisteredTrackListed`, `...UnknownTrackGetsGenericRow`,
  `...BuiltInsAllRegistered`, `...DetailsProxyAppliesEditThroughModel`, `...AddTrackInitializesDefaults`,
  `...TwoToolkitsOneAssetFocusesOpen`.
- **Acceptance**: a `UGSTN_CombatTimeline` asset opens in the toolkit; every M3 track kind can be added, moved, resized, deleted and
  undone with no details-panel frame typing; Save writes the asset; closing and reopening restores the layout.
- **E5b (separate commit, after M4-hitbox and M4-hurtbox)**: `GSTN_HitboxTrackEditor`/`GSTN_HurtboxModifierTrackEditor` + their
  `IGSTN_KeyedTrackAccess` implementations (add/move/duplicate/paste keys with interpolated values from the **same pure evaluation
  function the runtime uses**), tests `GSTNCombat.Editor.Model.AutoKeyCreatesInterpolatedKey`, `...HitboxKeysSortedAfterMove`,
  `...DuplicateKeysAcrossFrames`. Size S.
- **Spec**: S §5.1, §5.2, §12. **Size**: L. **Needs**: M3-C2, C3 (E5); M4-hitbox, M4-hurtbox (E5b).

### E6 Preview scene

- **Plugin/module**: `GSTNCombatEditor` (+ `GSTNCombatCore` additions: `FGSTN_ShapeLines::Build(const FGSTN_HitShape&, FTransform)` in
  `GSTNCombatCore/Public/Shapes/GSTN_ShapeLines.h` if M4 has not already added line generation; pure, tested).
- **Files**: `Private/Preview/GSTN_TimelinePreviewScene.*` (`FAdvancedPreviewScene`, one `UDebugSkelMeshComponent`),
  `Private/Preview/GSTN_TimelineViewportClient.*` (`FEditorViewportClient`, camera presets side/front/3-4, follow-root, ground
  grid, lit/unlit; **gizmos come in E7**), `Private/Preview/GSTN_PreviewDrawing.*` (PDI drawing of hit/hurt boxes, root capsule,
  facing arrow, motion trail toggle, movement-path polyline from the Movement track velocity curve as an approximation; OQ Q5
  assumption: polyline only, toggle), `SGSTN_TimelineViewport.*`, toolbar combos for kit and skin. Editor-only properties
  `PreviewKit`/`PreviewSkin` (`TSoftObjectPtr`, `#if WITH_EDITORONLY_DATA`) on `UGSTN_CombatTimeline` (the second runtime type
  change of M5, listed in section 6) and in settings for inline mode.
- **Pose rule**: the pose is **set per frame, never ticked**: `Position = SectionStart + (Frame / TickRate) x MontagePlayRate`
  (`SetPosition`, `TickAnimation(0)`, `RefreshBoneTransforms`); one function `FGSTN_PreviewPose::Evaluate(Frame)` is shared with the
  baker (E8) so what you see is what is baked. No montage: reference pose.
- **Overlays (need M4)**: hurtboxes of the kit's `HurtboxSet` coloured by `EGSTN_HurtState` at the playhead frame with
  `ShapeOverrides` applied; hitboxes active at the frame coloured by `HitGroup`, selected one thicker; baked-socket hitboxes use the
  baked transform (live socket plus a stale marker when missing). Without M4 the scene shows mesh + pose + root capsule + facing
  arrow, and the overlay code compiles behind the registry (no `#if`).
- **Tags**: none. **Tests** (`GSTNCombat.Editor.Preview.*`): `PoseAtFrameMatchesSectionTime` (pose bone at frame f equals the montage
  evaluated at `f / TickRate`; `NeedsRHI` tag if the scene needs it), `EvaluateIsRepeatable` (3 calls identical),
  `ShapeLinesSphereCapsuleBox` (vertex counts, bounds; Core test `GSTNCombat.Core.ShapeLines.*`), `FollowRootFramesMesh` (math only).
- **Acceptance**: opening a timeline with a montage shows the pose scrub to the frame; kit/skin changes persist per asset
  (`PreviewKit`/`PreviewSkin`) and survive restart; no per-frame allocation in `Draw`.
- **Spec**: S §5.4. **Size**: L. **Needs**: M3-C2 (montage section), M3-C5 (skin mesh in `UGSTN_SkinDefinition`); overlays M4-hurtbox,
  M4-hitbox, M4-shapes. **S1 result must be recorded first.**

### E7 Gizmos and viewport picking

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Private/Preview/GSTN_TimelineViewportClient.*` extended (`FWidget`: translate `W`, rotate `E`, scale `R`;
  `GetWidgetLocation`, `GetWidgetCoordSystem` local/world toggle, `InputWidgetDelta` converting the world delta back into the
  key's space at the playhead frame, then `SetKeyTransform`), `Private/Preview/GSTN_HitboxHitProxy.h` (`HGSTN_HitboxProxy`),
  spaces `Root`, `BakedSocket` (baked, else live), `WorldAtMoveStart` (preview origin), `Projectile` (identity in M5);
  **auto-key** (drag on a frame without a key creates the key with interpolated values first, one transaction; OQ Q6
  assumption: gizmo + auto-key on by default, numeric fields always available), add key at playhead (`K`), copy keys across
  frames, offset all keys (Shift+drag), mirror key across the root YZ plane; shape handles map scale to `Extent` (uniform for
  spheres), never the actor scale; click-to-select a hitbox in the viewport highlights its row and fires `OnKeySelected`
  (also the hook for `GSTNPartsEditor`, S §12b). All gizmo code stays in this one class (S §15 risk 3; apply the S4 fallback if used).
- **Tags**: none. **Tests** (`GSTNCombat.Editor.Gizmo.*`): `DeltaToKeySpaceRoundTrip` (world delta -> key delta -> world equals input within
  1e-4 for every space and a rotated root), `ScaleMapsToExtentNotActorScale`, `AutoKeyOneTransaction`, `AutoKeyOffEditsNearestKey`,
  `PickingRayHitsNearestShape` (ray vs shape maths, widget-free), `MirrorAcrossRootYZ`.
- **Acceptance**: a hitbox key is moved, rotated and scaled with the gizmo; each drag is one undo step; Escape cancels; clicking the
  shape selects the row; manual smoke recorded.
- **Spec**: S §5.5. **Size**: M. **Needs**: M4-hitbox; S4 result.

### E8 Socket baking and staleness

- **Plugin/module**: `GSTNCombatEditor` (+ one editor-only field in `GSTNCombatCore`).
- **Files**: Core: `FGSTN_BakeInfo { uint32 InputHash; float TickRateHz; FSoftObjectPath Mesh; FDateTime When; }` added to
  `FGSTN_CombatTimelineData` under `#if WITH_EDITORONLY_DATA` (the third runtime type change of M5, section 6); guard
  `NetSerialize`/copy/hash code so cooked builds compile without it. Editor: `Public/Bake/GSTN_SocketBaker.h`,
  `Private/Bake/GSTN_SocketBaker.cpp` (`FGSTN_SocketBaker::Bake(Timeline, PreviewMesh, Rate) -> FGSTN_BakeResult`; S §7.2 steps 1..6:
  distinct sockets from `Space == BakedSocket` hitboxes and HurtboxModifier overrides; missing socket = error naming the hitbox and
  the previous bake kept; per-frame component-space socket transform converted to **root space** with the mesh component's relative
  transform; scale 1; quantize with the same function the runtime reads; one transaction replacing `BakedSockets` and `BakeInfo`),
  `Public/GSTN_CombatEditorUtils.h` (`UGSTN_CombatEditorUtils::GetTickRateHz()` per S2, `BakeTimeline`, `IsBakeStale` as
  `BlueprintCallable, meta=(DevelopmentOnly)`), `Private/Bake/GSTN_BakeHash.cpp` (hash of montage raw-data GUIDs per underlying
  sequence of the section, section name and start, play rate, `TotalFrames`, tick rate, preview mesh path and skeleton GUID, the
  sorted socket list), toolbar **Bake sockets** (yellow dot + tooltip when stale, `Ctrl+B`), stale check on load and refresh
  (GUIDs only), opt-in *Bake on save* (OQ Q8 assumption: manual by default). **Bake all** moves to E11.
- **Tags**: none. **Tests** (`GSTNCombat.Editor.Bake.*`): `FrameAtFEqualsPoseAtFOverRate` (within 1e-3 cm), `FrameCountEqualsTotalFrames`,
  `TwoBakesBitIdentical`, `TickRateChangeMarksStale`, `PlayRateChangeMarksStale`, `SocketListChangeMarksStale`,
  `MissingSocketIsErrorKeepsPreviousBake`, `OneUndoRestoresPreviousBake`, `CookedStructHasNoBakeInfo` (struct layout test or a build with
  `WITH_EDITORONLY_DATA=0` in the Core test config) and Core `GSTNCombat.Core.TimelineData.BakeInfoNotInNetSerialize`.
- **Acceptance**: S §14 item 4 steps; baked frames equal the preview pose; changing montage, play rate, sockets or tick rate shows the stale dot;
  M3-C10 confirmation recorded: the `PlaySyncedMontage` start (blend-in, play rate, section) matches the bake assumption (if not, adjust
  the shared `FGSTN_PreviewPose` and stop-and-ask only when the runtime must change); the M4 harness `scenario_hit_basic` re-run with a
  baked timeline stays identical (listen then dedicated, 60/15, 2 % loss; this checkpoint touches Core data).
- **Spec**: S §7. **Size**: M. **Needs**: M4-baked, M4-hitbox; S1, S2; C10 for the final confirmation.

### E9 Hurtbox set editor, generator, mirror

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Private/Hurtbox/GSTN_HurtboxSetEditorToolkit.*` (`FAssetEditorToolkit` for `UGSTN_HurtboxSetDefinition`, same preview
  scene, tree/list of hurtboxes with name, bone, tags, `Priority` (P-hurtbox-priority) in details, gizmos on the selected shape,
  undo as in E2, editor-only `PreviewMesh` soft pointer on the hurtbox set falling back to the preview kit's skin: the fourth
  runtime type change, section 6), `Private/Hurtbox/GSTN_HurtboxGenerator.*` (pure functions
  `FromPhysicsAsset(UPhysicsAsset*, USkeletalMesh*, Options)`, `MirrorHurtbox`, naming-pair matching), asset definition for the
  hurtbox set, live findings (duplicate names, empty extents, shape outside a sane radius, bone absent).
- **Generator rules** (S §8): sphyl -> Capsule, sphere -> Sphere, box -> Box, convex/tapered capsule -> bounding capsule along the
  longest axis with a warning; transform = bone reference-pose component-space x primitive local x mesh relative transform,
  root-relative (same frame as the baker); name = bone name (`bone_N` for several primitives); `Combat.Hurtbox.*` tag suggested from
  the settings mapping, empty if unmapped; options: skip kinematic bodies, scale margin, merge small bodies; regeneration keeps
  hand edits matched by name unless **Replace all** (OQ Q10 assumption); mirror L<->R by settings pairs (`_l/_r`, `Left/Right`,
  `.L/.R`) through the root YZ plane (forward axis in settings, OQ Q11 assumption), creates or overwrites with confirmation,
  unpaired bones reported never guessed.
- **Tags**: uses E1's `Combat.Hurtbox.*`. **Tests** (`GSTNCombat.Editor.Hurtbox.*`): `FromPhysicsAssetCountsPerPrimitive`,
  `ConvexFallsBackToCapsuleWithWarning`, `RootRelativePositions`, `RegenerationKeepsHandEdits`, `ReplaceAllDiscardsHandEdits`,
  `MirrorCreatesCounterpart`, `MirrorReportsUnpairedBones`, `MirrorOverwriteNeedsConfirmation`, `SuggestedTagFromMapping`,
  `ToolkitUndoRestoresShape`.
- **Acceptance**: S §14 item 5: set generated from the test character's Physics Asset, mirrored, edited with gizmos; counts
  match the bodies; regeneration keeps hand edits.
- **Spec**: S §8. **Size**: M. **Needs**: M4-hurtbox, M4-shapes, P-hurtbox-priority; E6/E7 for scene and gizmos.

### E10 Validation mechanism and M3-only rules, kit/weapon/preset validators

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Public/Validation/GSTN_ValidationReport.h` (`FGSTN_ValidationFinding { Severity, Code, Message, Asset, Where }`,
  `FGSTN_ValidationReport`, `EGSTN_Severity`, `FGSTN_FindingLocation`), `Private/Validation/GSTN_CombatValidators.*`: pure rule
  functions `Check<Rule>(const FGSTN_CombatTimelineData&, FGSTN_ValidationReport&)` and `UEditorValidatorBase` children
  `UGSTN_TimelineValidator`, `UGSTN_KitValidator`, `UGSTN_WeaponValidator`, `UGSTN_LoadoutPresetValidator`,
  `UGSTN_AbilityTimelineValidator` (inline timelines through the CDO). Errors invalidate, warnings do not (OQ Q12 assumption).
- **M3-only rules in this checkpoint** (codes `Timeline.*`): `FrameRange`, `MontageLength` (warning), `OverlapSameKind` (Movement, Tags
  conflicts; the Hitbox same-group part waits for E11), `RootMotionMix` (warning), `DeferredCommit`, `CancelWindowTarget`, `TagsTrack`,
  `AimTurn`, `UnknownTrack` (warning). Reuses `GSTN_TimelineValidation.h` from M3-C2 (the editor wraps it, no duplicate logic).
- **Kit/weapon/preset**: UI over existing validators (no new logic): M1 slot validator `FindConflicts` (`Ambiguous`, `DuplicateCommand`,
  `InvalidSlot` errors, `Shadowed` warnings) over the compiled command set of each kit, each weapon on top of the kit and each
  loadout preset, plus `UGSTN_LoadoutRules` of M3 (allowed weapons/supers/skins, one stats GE per weapon, bindings to removed
  commands, timelines present in the registry); each conflict finding carries both slot names and a **Show** action printing the
  patterns in the M1 §4 notation; a conflict on a preset names the weapon or super that introduces it; kit/weapon identity-leaf
  rule of M3-C5 is reported (exactly one leaf under `Kit.`/`Weapon.`).
- **Tags**: none (tags read only). **Tests** (`GSTNCombat.Editor.Validate.*`): one failing + one passing in-code timeline for each rule
  above, `KitConflictFlagsBothSlotNames`, `PresetConflictNamesWeaponOrSuper`, `FixClearsTheError`, `UnregisteredTagIsError`,
  `ErrorsInvalidateWarningsDoNot`, `ReportIsWidgetFree` (no Slate include in `GSTN_ValidationReport.h`).
- **Acceptance**: S §14 item 3: a deliberately conflicting slot is flagged `Ambiguous` on the kit and on the preset with both slot
  names and notation; Data Validation runs on save and by `-run=DataValidation` (OQ Q13: command available, CI optional).
- **Spec**: S §9.1, §9.2. **Size**: M. **Needs**: M3-C2, C5, C6, M1 validators. **Can start after M3 only.**

### E11 M4 validation rules, validation panel, Bake all

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: remaining rules in `GSTN_CombatValidators.cpp`: `OverlapSameKind` (Hitbox same-group same-shape warning), `BakedStale`,
  `BakedSocketMissing`, `BakedLength`, `HitboxKeys`, `HitData`, `HurtState`, `Variants`; hurtbox-set validator
  `UGSTN_HurtboxSetValidator` (duplicate names, extents <= 0, bone absent as a warning, `Combat.Hurtbox` tag outside the root, no
  hurtbox = error); `Private/Validation/SGSTN_ValidationPanel.*` (nomad tab Window > GSTN Combat > Validation; tree grouped by
  asset with severity icons; double-click opens the asset, selects the track/key and sets the playhead; *Validate all* over the
  Asset Registry; Message Log category `GSTNCombat`); timeline toolkit *Frame Data* tab shows findings continuously (debounced
  250 ms) and lanes draw red/yellow markers at the offending frame; **Bake all** (`gstn.Combat.BakeAll`, menu entry, content browser
  folder context menu; loads unloaded inline Blueprints under a slow task) and `gstn.Combat.ValidateAll`; the Tools/Window menu
  registration through `UToolMenus`. Stale bakes are a cook-blocking **error** but do not block editing (S §2.1 item 7).
- **Tags**: none. **Tests**: remaining rule pairs as in E10 (`GSTNCombat.Editor.Validate.<Rule>`), `BakeAllClearsAllStale`,
  `PanelFindingLocationSelectsKey` (location -> selection function), `MessageLogReceivesFindings`, `ValidateAllOverRegistryFindsNestedAssets`.
- **Acceptance**: each code of S §9.2 has a failing and a passing test; the panel lists findings of all asset kinds; *Bake all*
  clears every `BakedStale`.
- **Spec**: S §9.1, §9.2, §7.3. **Size**: M. **Needs**: E10 and M4-hitbox, M4-hitdata, M4-hurtbox, M4-baked; E8.

### E12 HitData customization and knockback preview

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Private/Timeline/Details/GSTN_HitDataCustomization.*` (`IPropertyTypeCustomization` for `FGSTN_HitData`: damage/knockback
  grouped, **angle dial** with the resulting direction on the movement plane, special angle and yaw source with short
  descriptions, hit type as a tag combo, stagger force/hitstop fields grouped, extension data shown through the standard instanced
  struct editor (so `GSTNPartsEditor` can customize `FGSTN_PartHitData`, S §12b)), a *Preview knockback* line calling the active
  `UGSTN_KnockbackModel` with a test victim (percent and weight fields in the toolbar; OQ Q7 assumption: one computed line, no dummy
  victim) and the reaction tier from `FGSTN_ReactionTuning`; registration in the module.
- **Tags**: none (hit type tag picker uses `Categories`). **Tests**: `GSTNCombat.Editor.HitData.AngleDialMapsToAngleDeg`,
  `...KnockbackPreviewMatchesModel` (preview value equals the model on the same inputs, same function, no copy),
  `...ExtensionDataNotHiddenByCustomization`.
- **Acceptance**: hit data editable with the dial and the preview line; values agree with `UGSTN_KnockbackModel` on a hand case.
- **Spec**: S §5.1, M4 §3. **Size**: S/M. **Needs**: M4-hitdata, M4-knockback.

### E13 Inline mode (Blueprint timelines)

- **Plugin/module**: `GSTNCombatEditor`.
- **Files**: `Private/Timeline/Details/GSTN_TimelineDataCustomization.*` (`IPropertyTypeCustomization` for `FGSTN_CombatTimelineData`: **Open
  Timeline Editor** button + frame-data summary line; same toolkit in inline mode), real implementation of the inline branch of
  `FGSTN_TimelineEditContext` (resolves `Timeline` on the CDO of the Blueprint's `GeneratedClass`, re-resolves on
  `OnBlueprintCompiled`, closes with a message if the property is gone, `FBlueprintEditorUtils::MarkBlueprintAsModified`,
  `GetObjectsCurrentlyBeingEdited` returns the Blueprint so Save saves it), and an **Extract to timeline asset** command (fallback of
  S §15 risk 4, OQ Q2 assumption: assets recommended, inline supported, extract if brittle).
- **Tags**: none. **Tests** (`GSTNCombat.Editor.Inline.*`): `ContextResolvesCdoTimeline`, `SurvivesBlueprintCompile`, `ToolkitClosesWhenPropertyGone`,
  `EditMarksBlueprintModified`, `UndoRestoresCdoValue`, `ExtractCreatesAssetAndClearsInline`, `TwoToolkitsSameBlueprintRefused`.
- **Acceptance**: S §14 item 7: same flow on the inline timeline of a `UGSTN_GA_BaseCombat` Blueprint; Blueprint marked modified,
  survives compile, Save writes the Blueprint. **If the reinstancing test is brittle, ship read-only preview + extract and say so in
  the commit** (no stop-and-ask: this is the spec's own fallback). Needs a throwaway test Blueprint created in `Saved/`/test content, never `BP_GASNPP_TestCharacter`.
- **Spec**: S §2.1 item 3, §4.1. **Size**: M. **Needs**: M3-C10.

### E14 `GSTNCombatDebug` input display

- **Plugin/module**: `GSTNCombatDebug`.
- **Files**: `Public/Widgets/SGSTN_InputDisplay.h`, `Private/Widgets/SGSTN_InputDisplay.cpp`, `Private/Widgets/GSTN_InputDisplayLayout.*` (pure
  layout function: one column per frame at the NPP tick rate; row 1 buttons press/hold/release; row 2 stick as numpad digit
  with facing-relative direction; flick `!`; frame gap in M1 §4 notation `7:+A`; consumed presses greyed with the command tag on
  hover; `Input.Event.CommandExecuted` marker), scripted history via `FGSTN_InputScript::Build` (type a notation, see which
  command of the open kit resolves), PIE corner overlay (`gstn.Combat.Debug.InputDisplay 1`), `IGSTN_CombatDebugModule::CreateInputDisplay()`
  implemented; editor side: nomad tab *Window > GSTN Combat > Input Display* in `GSTNCombatEditor` via
  `FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>` (absent module = tab disabled with a message, OQ Q16/Q15
  assumption: development tool, never Shipping).
- **Const accessors**: if not yet present, `UGSTN_CombatSimComponent::GetInputHistoryView()` const (M2 type; this is the first of the
  const-accessor additions, listed in section 6) used only by the widget, never inside the simulation.
- **Tags**: reads `Input.Event.CommandExecuted` only. **Tests** (`GSTNCombat.Debug.InputDisplay.*`, no world): `LayoutOfSampleHistory` (the spec string
  `0:face=90 0:4t 6:6t! 7:+A`: columns, gaps, greyed press), `ConsumedPressGreyed`, `FlickMarker`, `ScriptedHistoryResolvesCommand`,
  `LayoutIsPure` (no Slate in the layout header); editor test `GSTNCombat.Editor.Debug.TabSkipsWhenModuleAbsent`.
- **Acceptance**: in PIE (listen server: host + 2 clients) the overlay shows the **local** player on each machine, including the host,
  with correct frame gaps; with the debug plugin disabled the editor builds, the tab says the module is missing, nothing else changes.
- **Spec**: S §10.1. **Size**: M. **Needs**: M2 (history), M1 notation; scripted lookup M3-C6. **Can start after M2.**

### E15 Gameplay Debugger category, const accessors, ring of hits

- **Plugin/module**: `GSTNCombatDebug` (+ read-only accessors in the sim component, `GSTNCombatNP`).
- **Files**: `Private/GameplayDebugger/GSTN_GameplayDebuggerCategory.h/.cpp` (`FGameplayDebuggerCategory` subclass, guarded by
  `WITH_GAMEPLAY_DEBUGGER_MENU`, default key `6`, configurable, registered through `IGameplayDebugger::RegisterCategory`;
  `CollectData` on the authority for the selected pawn, replicated via the category data pack, `DrawData` on the canvas), data
  struct sections for combat state (action state, active timeline with frame/FAF/total, hitstop/hitstun remaining, active weapon,
  loadout revision, lock tags, adoption pending), input (last N events in notation, buffered command, last resolved command and
  why), active windows now (cancel, override, `Tags`, hurt state), the last 8 hits dealt/received from a ring in the debug module
  (frame, other entity, hit data id, damage, knockback, relationship; fed by `UGSTN_SimEventSubsystem::Listen`, never sync state);
  `GSTNCombatNP`: read-only const accessors on `UGSTN_CombatSimComponent` (timeline frame, windows, registry names, action state;
  they never run inside the simulation: grep gate in the commit) and nothing else.
- **Host rule (S §10.2b, DD §6b)**: the category collects on the authority, which is the host's process; selecting a pawn on the
  host works as on a dedicated server; clients get data through the data pack; the **ring is fed once on the host** (the listen
  host sees outbox events once, not twice); off by default (`gstn.Combat.Debug.*` = 0) with 40+ entities; Development-only.
- **Tags**: reads `Combat.State.*` and `Combat.Lock.Movement` to list active tags, read-only. **Tests** (`GSTNCombat.Debug.Category.*`):
  `CollectWritesExpectedFieldsFromBuiltComponent` (non-world part), `RingHoldsLastEightHits`, `RingFedOncePerEventOnHost` (listen:
  host-local event counted once), `RingTrimmedOnRollback` (re-simulated events not double counted), `NoWorldAccessorsAreConst`
  (compile-time check). Harness: `combat_spike`, `combat_lock`, `scenario_hit_basic`, `scenario_hit_host` (M4) with the category
  enabled in a Development PIE (listen first, dedicated second, 60/15 ms, 2 % loss): **identical state snapshots with the
  debugger on and off, no extra reconciles** (the category must not perturb the simulation).
- **Acceptance**: S §14 item 8 for the category: correct data on the host and on clients in a listen-server PIE, and on a dedicated
  server with two clients (the authority's category shows the client pawn).
- **Spec**: S §10.2, §10.2b. **Size**: M/L. **Needs**: M3-C6, C8..C10 (skeleton, M3 sections); M4-hitstop-state, M4-events,
  M4-reaction (hit sections).

### E16 Debug draw and BakeCheck

- **Plugin/module**: `GSTNCombatDebug`.
- **Files**: `Public/Draw/GSTN_CombatDebugDraw.h`, `Private/Draw/GSTN_CombatDebugDraw.cpp` (CVars `gstn.Combat.Debug.Hitboxes`, `.Hurtboxes`,
  `.Buffer`, `.InputDisplay`, `.BakeCheck`; draw functions using `FGSTN_ShapeLines::Build` from Core and `DrawDebugLine`, shapes
  identical to the editor preview; from the world snapshot (M4-snapshot): hitboxes by group, hurtboxes by state, contact points for
  0.5 s; the same function runs on the machine that simulates, including the listen-server host, non-shipping only); `BakeCheck`
  overlays baked socket data on the live socket (S §15 mitigation of the S1 risk).
- **Tags**: none. **Tests**: `GSTNCombat.Debug.Draw.LinesMatchEditorLines` (same Core function, equal vertex lists), `...CVarsDefaultOff`,
  `...NoDrawInShippingConfig` (build define check), `...BakeCheckReportsDivergence` (synthetic live vs baked transforms). Harness: the draw is
  enabled during `scenario_hit_basic`; no change in snapshots.
- **Acceptance**: PIE shows boxes that match the editor preview frame by frame; BakeCheck shows zero divergence for a fresh bake.
- **Spec**: S §10.3. **Size**: S/M. **Needs**: M4-snapshot, M4-shapes, M4-hitbox, M4-hurtbox; E6 for `FGSTN_ShapeLines`.

### E17 Trace channel and sink

- **Plugin/module**: `GSTNCombatDebug` + a tiny hook in `GSTNCombatNP`.
- **Files**: `GSTNCombatNP/Public/Debug/GSTN_CombatTraceSink.h` (`IGSTN_CombatTraceSink`, set by the debug module, null in Shipping;
  calls are `#if !UE_BUILD_SHIPPING` and outside the sync state), the emit sites in the sim tick (`CombatFrame` per frame per entity:
  entity, frame, resimulated flag, action state, timeline index and frame, hitstop, hitstun, history head; `CombatEvent`: command
  executed, hit, adoption, cancel, rollback frame range with `TRACE_BOOKMARK` for rollbacks), `GSTNCombatDebug/Public/Trace/
  GSTN_CombatTrace.h` and `Private/Trace/GSTN_CombatTrace.cpp` (`UE_TRACE_CHANNEL`, `UE_TRACE_EVENT_BEGIN`). S3 result decides what
  Insights shows.
- **Tags**: none (codes are `FName`/ints, not tags). **Tests** (`GSTNCombat.NP.Trace.*` / `GSTNCombat.Debug.Trace.*`): `SinkNullIsNoOp`,
  `SinkReceivesFramePerEntity`, `ResimulatedFlagSetOnRollback`, `NoSyncStateChange` (state hash with and without a sink identical),
  `ShippingDefineStripsCalls`. Harness: full M2/M3 scenario set listen + dedicated, 60/15, 2 % loss with the channel on: **no change in
  reconciles, sim time per frame within noise (record)**.
- **Acceptance**: a capture in Unreal Insights shows `CombatFrame` rows and rollback bookmarks for a forced-rollback run
  (`forced_rollback`) on the host and a client; no NP dependency on the debug module (checked by grep).
- **Spec**: S §10.4. **Size**: M. **Needs**: M3-C8..C10 (NP loop); M4-events for the hit events; S3.

### E18 Rewind Debugger track (stretch, only if S3 says yes)

- **Plugin/module**: `GSTNCombatDebug` (an optional editor-side module of the same plugin if the Rewind Debugger extension is
  editor-only: decided by S3, otherwise a new `GSTNCombatDebugEditor` inside the plugin).
- **Files**: analyzer, provider, track creator, track per entity with timeline and state transitions, re-simulated frames hatched.
- **Tests/acceptance**: `GSTNCombat.Debug.Rewind.*` provider unit tests with a recorded trace file; manual capture opens in the
  Rewind Debugger. **Skipped** (recorded in the report) when S3 shows the NP pawns are not recorded or the cost exceeds the agreed
  budget (OQ Q14: ask the owner).
- **Spec**: S §10.4 M5.8. **Size**: L. **Needs**: E17, S3.

### E19 Extension procedure, parts hooks, designer scenario, closing

- **Plugin/module**: `GSTNCombatEditor`, test content, docs.
- **Work**: (1) **Extension procedure of S §12** run once with a throwaway `FGSTN_ShieldTrack` (a window struct in a scratch test
  module or in test content code) to prove acceptance 9: new track in under an hour, no widget code touched; the throwaway is
  removed and the procedure text copied to the README. (2) **Parts hooks audit and test** (S §12b): public headers of the preview
  scene, the shape drawing, the hurtbox toolkit/generator, the validation mechanism, the track registry and `OnKeySelected` are
  exported without parts names; `GSTNCombat.Editor.Hooks.ExternalClientCompiles` (a test-only external client implements a fake
  `FGSTN_HitExtension` customization and a fake validator that registers into the panel category) proves `GSTNPartsEditor` can plug in
  with no change to M5; **no** reference to `GSTNParts` in `GSTNCombatEditor` (grep gate in the commit); if M4b already has its
  editor module, a smoke run of the part overlay on the shared preview scene (colour per part) is added, otherwise recorded as owed
  by M4b. (3) **Designer scenario**, editor only, no Python, no details frame typing: three-hit jab string (a `Root` hitbox, a
  `BakedSocket` hitbox on the weapon hand, an ally variant), cancel window jab 1 -> jab 2 on `OnHit` and `Always`, a
  `HurtboxModifier` armor window on jab 3, and an EX version (second ability, shared timeline with a different hitbox size);
  frame data shown; undo/redo restores every step; the same flow on an inline timeline. Content under
  `/Game/GASNPPTest/Combat/Editor/` (test content only; nothing edits `BP_GASNPP_TestCharacter`), Python smoke
  `Tools/GASNPPTest/gstn_editor_check.py` (open, bake, validate, print report). (4) Run `scenario_hit_basic` with the editor-authored
  timeline and with the hand-built one: **identical snapshots** (60/15 ms, 2 % loss, listen then dedicated; acceptance 6). (5)
  Isolation: CI/matrix builds (editor disabled, debug disabled, both disabled, Shipping packaged build without both plugins),
  grep of every `.Build.cs`/`.uplugin`. (6) One docs commit: combat.md §3, §14, §17 (spike results), roadmap plugin names,
  testing.md line (`Automation RunTests GSTNCombat.Editor`), both READMEs final.
- **Tests**: `GSTNCombat.Editor.Hooks.*`, `GSTNCombat.Editor.Registry.ThrowawayTrackListed` (removed with the throwaway), full
  `GSTNCombat.*` run.
- **Acceptance**: S §14 items 1, 2, 6, 7, 8, 9, 10 all ticked in the commit message.
- **Spec**: S §12, §12b, §13, §14. **Size**: M.

## 5. Assumptions for unanswered owner questions

Question ids refer to OQ (M5 section, Q1..Q20 in order). Answered: Q18 (zone tags are not only hurtbox tags; definitive taxonomy from
combat-parts) and Q20 (parts tools live in `GSTNPartsEditor`, DD §1/§6b), both followed.

| Q | Assumption planned on | Affects | Cost to change |
|---|---|---|---|
| Q1 Who uses the editor | A technical designer who knows Unreal, not code; messages in plain language, destructive actions confirm | E4, E5, E9, E11 (wording, confirmations) | Low: strings and confirmations |
| Q2 Assets vs inline | Assets recommended; inline supported with the same toolkit plus an **Extract to timeline asset** fallback | E13 | Low: E13 can shrink to read-only + extract |
| Q3 Frame-data prominence | Phases always visible, read only; first active frame = index + 1; no on-hit/on-block advantage (needs M6) | E3, E4 | Low |
| Q4 Copy/paste scope | Keys only, via text clipboard; no whole-track copy in v1 | E2 | Medium: clipboard format and compatibility checks |
| Q5 Preview scope | Pose only plus an approximate movement polyline (toggle); truth stays in the game | E6 | Low |
| Q6 Gizmo vs numeric, auto-key | Gizmo with auto-key on by default; numeric always available | E7 | Low (setting) |
| Q7 Victim comparison in preview | One knockback line in details; no dummy victim mannequin | E12 | Medium if a victim bench is later wanted |
| Q8 Bake trigger | Manual; *Bake on save* off by default; stale = cook-blocking error | E8, E11 | Low |
| Q9 Montage length change | Never automatic; warning bar with an undoable **Fit Total Frames** | E5, E10 | Low |
| Q10 Hurtbox regeneration | Regeneration keeps hand edits by name; **Replace all** explicit | E9 | Medium (merge logic) |
| Q11 Bone naming and forward axis | Settings pairs `_l/_r`, `Left/Right`, `.L/.R`; forward axis in settings; no guessing, unpaired reported | E9 | Low |
| Q12 Blocking errors | Error: out-of-range intervals, stale bake, ambiguous slots, unregistered tags; the rest warnings | E10, E11 | Low (severity table in code) |
| Q13 CI validation | Panel by hand plus `-run=DataValidation`; CI optional | E10, E11 | Low |
| Q14 Debug priority | Input display and Gameplay Debugger first; trace via Insights; Rewind Debugger only if S3 allows | E14, E15, E17, E18 | E18 cost is the issue: ask after S3 |
| Q15 Input display for players | Development tool, CVar overlay, never in Shipping | E14 | Medium: a player-facing version needs art and a mode |
| Q16 Two plugins | `GSTNCombatEditor` and `GSTNCombatDebug`, nobody depends on them; editor -> debug dynamic and optional | E1, all | Medium: rename/merge touches Build.cs files and the matrix |
| Q17 Editor and NP | Editor/debug depend on `GSTNCombatNP` (tick rate, accessors); revisit with M9 (stock GAS adapter) | E1, E8, E15, E17 | Medium: tick-rate source and accessors would need an abstraction |
| Q19 Test tags | Test tags in the project test content ini, not in the plugins | E1, E19 | Low |
| Extra: runtime type edits | Adding editor-only fields (`FGSTN_BakeInfo`, `PreviewKit/PreviewSkin`, `PreviewMesh`) to M3/M4 types is acceptable (section 6) | E6, E8, E9 | Low: move to an editor-only side asset |

## 6. Blueprint and shared-asset edits that need the owner's OK before they happen

1. **`GASP_Giustino2.uproject`**: add the two plugins (`GSTNCombatEditor` Editor-only, `GSTNCombatDebug`) in E1. Announced, no behaviour change.
2. **Runtime plugin types gaining editor-only fields** (E6, E8, E9; all `WITH_EDITORONLY_DATA`, stripped from cooked data): `PreviewKit`/`PreviewSkin` on `UGSTN_CombatTimeline`,
   `FGSTN_BakeInfo` on `FGSTN_CombatTimelineData`, `PreviewMesh` on `UGSTN_HurtboxSetDefinition`. Existing assets load unchanged; saving rewrites them
   once. If the owner prefers zero change to runtime types: keep these in an editor-side `UAssetUserData` and accept the hash lives off the data (cost: E8 reread).
3. **Const accessors on `UGSTN_CombatSimComponent`** (E14, E15) and `IGSTN_CombatTraceSink` plus emit calls in the NP tick (E17): touches M2/M3 NP code, no sync state change, PIE gated.
4. **`GSTNCombatCore` additions**: `FGSTN_TimelinePhases` (E3) and `FGSTN_ShapeLines` (E6), pure.
5. **`GSTNCombatGAS` tags**: `Combat.Hurtbox.*` (E1).
6. **Project test content** under `/Game/GASNPPTest/Combat/Editor/`, `Config/Tags/GSTNCombatEditorTest.ini`, `Tools/GASNPPTest/gstn_editor_check.py` (E19). No edit of
   `BP_GASNPP_TestCharacter` or any existing Blueprint; the inline-mode test uses its own Blueprint. If a step needs the character Blueprint (for a hurtbox set reference), ask first.
7. **Docs commit** (E19): combat.md §3, §14, §17, roadmap, testing.md. Only after the owner lifts the `docs/` rule for commits.
8. **Spike S1 fallback** (raw-animation baking) and **S3 Rewind Debugger cost**: owner decisions as in section 3.

## 7. Risks and parallelisation hints

### 7.1 Risks

- **Custom Slate cost** (E4/E5, S §15): largest item; mitigation: model/view split and a usable cut after E4+E5 with the details panel.
- **Montage evaluation fidelity** (S1, E6, E8): the editor pose must equal `PlaySyncedMontage`; `BakeCheck` (E16) exposes any divergence in PIE;
  confirm at C10 in E8.
- **Gizmos on non-components** (S4, E7): isolate in one class; dummy-component fallback.
- **Inline Blueprint timelines** (E13): CDO and reinstancing; fallback read-only + extract.
- **Engine API drift** (`StructUtils`, `AdvancedPreviewScene`, Rewind Debugger, Gameplay Debugger, `UAssetDefinitionDefault`): verify each against 5.8 headers
  before each checkpoint; keep API use behind our own interfaces (`IGSTN_TrackEditor`, model).
- **Data model drift with M3/M4**: M4 is not final; E5b, E6 overlays, E7..E9, E11, E12 are written against M4 headers once they exist: re-read the finished headers and fix
  `combat-m5-editor.md` first when they differ (S status note). Only public structs and pure evaluation functions are used.
- **Editor-only field in a runtime struct** (E8): `NetSerialize`/copy/hash guard; Core tests in a configuration with `WITH_EDITORONLY_DATA=0`.
- **Debug leaking into Shipping** (E14..E17): `UE_TRACE`, debugger macros, the Build.cs exclusion; the packaged Development and Shipping builds are in the E19 gate.
- **Debug overhead and heisenbugs** (E15, E17): collecting on the host shares the simulation's frame; assert identical snapshots with the tooling on and off.
- **Listen host specifics**: no `OnRep` on the host (the debugger must not rely on it), host events once, host has no prediction (the trace marks no
  resimulation for the host pawn); tested in E15/E17.
- **Time**: E4, E5, E6, E15 are the long ones; E18 is optional.

### 7.2 Parallelisation hints (separate threads)

- **Thread A (Slate timeline, needs M3 C2 + C5)**: E1 -> E2 -> E4 -> E5 -> (after M4) E5b -> E7 -> E12. E3 is a small Core task that fits at any point after C2.
- **Thread B (validation, needs M3 C2/C5/C6)**: E10 right after E1; E11 after M4 data. Independent of Thread A once E1 and the report types exist.
- **Thread C (debug plugin, needs M2, then C10)**: E14 can start right after M2 (only needs E1's debug skeleton); E15 skeleton after C10; E17 after C10.
  Touches `GSTNCombatDebug` and small NP accessors: **coordinate with the M3 NP owner** for the const accessors and the trace sink hook (merge conflicts in the sim component).
- **Thread D (preview + bake + hurtbox, needs M4 data)**: E0 (S1, S2, S4) can run now as a research thread; E6 pose part after C5; then E8, E9 after M4-baked/M4-hurtbox.
- **Serial by nature**: E1 first; E2 before E4/E5; E6 before E7, E9, E16; E8 after E6 (shared pose function); E10 before E11; E13 after E5; E19 last. E16 shares
  `FGSTN_ShapeLines` with E6 (if both threads run, define the helper once in the first checkpoint that needs it).
- **Needs the editor session (GUI/PIE)**: E0, E6 (RHI tests), E7 manual, E15, E16, E17 harness gates. **Headless-friendly**: E1..E3, E8 (math), E9 (generator), E10, E11 (rules), E14 layout.
