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
