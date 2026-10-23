# M5 spike report (E0)

Scratch notes of the M5 editor work, not design docs. Paths are relative to the project root. Date 2026-10-23.

## Summary

| # | Spike | Verdict |
|---|---|---|
| S1 | Montage sampling | PASS (test `GSTNCombat.Editor.Preview.Spike.MontageSampling`, real RHI): the pose can be set per frame; repeatable and backward-equal to 0 cm; root motion absent from the pose; about 1 to 8 ms per 60 frames x 3 sockets |
| S2 | Tick rate source | PASS by source read, confirmed by an editor test: readable from the settings CDO with no world, 62.5 Hz |
| S3 | Insights / Rewind Debugger | not done (needs PIE with the sim running) |
| S4 | Gizmo in a custom viewport | not done (needs an editor session) |

## S2 Tick rate source (source read, then confirmed by `GSTNCombat.Editor.Settings.TickRateFromNetworkPrediction`)

### Where the fixed tick rate lives

- The property is `FixedTickFrameRate` (float) in the struct `FNetworkPredictionSettings`, declared in
  `Plugins/GASNPP/NetworkPrediction/Source/NetworkPrediction/Public/NetworkPredictionSettings.h:29`, default `62.5`. Its comment
  says `Engine::FixedFrameRate` takes precedence when `bUseFixedFrameRate` is on; **nothing in the plugin implements that**
  (grep for `bUseFixedFrameRate` finds only the comment), so the engine setting is irrelevant here.
- The struct is held as the property `Settings` of `UNetworkPredictionSettingsObject`
  (`NetworkPredictionSettings.h:135-143`, `UCLASS(config=NetworkPrediction, defaultconfig)`), a plain `UObject` whose config
  lives in `DefaultNetworkPrediction.ini`, section `[/Script/NetworkPrediction.NetworkPredictionSettingsObject]`.
- The project sets it in `Config/DefaultNetworkPrediction.ini:2`: `FixedTickFrameRate=62.5`. The plugin's own
  `Plugins/GASNPP/NetworkPrediction/Config/DefaultNetworkPrediction.ini:2` says `60`; the project value wins, and the
  struct default is 62.5.
- At run time each `UNetworkPredictionWorldManager` copies the CDO into its own `Settings`
  (`NetworkPredictionWorldManager.cpp:50` and `:74-76`, `SyncNetworkPredictionSettings(GetDefault<UNetworkPredictionSettingsObject>())`)
  and every world pre-tick feeds `Settings.FixedTickFrameRate` to `OnWorldPreTick_Internal` (`:195`), which sets
  `FixedStepRealTimeMS = 1000 / rate` (float) and `FixedStepMS = (int32)FixedStepRealTimeMS` (`:208-209`). In the editor,
  `PostEditChangeProperty` re-syncs live managers (`NetworkPredictionSettings.cpp:15`). So the CDO is the source of truth
  for every world, and reading it needs no world.

### Readable with no world? Yes, by reflection only

`UNetworkPredictionSettingsObject` has **no export macro** (`class UNetworkPredictionSettingsObject : public UObject`), so another
module cannot name it (`GetDefault<>` would not link). The editor therefore finds the class by path and reads the property by
reflection: `FindObject<UClass>(nullptr, "/Script/NetworkPrediction.NetworkPredictionSettingsObject")`, its CDO, the struct
property `Settings`, the numeric property `FixedTickFrameRate`. This needs neither a world nor a link to the fork.

Implemented as `UGSTN_CombatEditorSettings::GetSimulationTickRate()` (`Plugins/GSTNCombatEditor/Source/GSTNCombatEditor/Private/Settings/GSTN_CombatEditorSettings.cpp`),
fallback `FallbackTickRateHz` (default 62.5) when the class is not loaded or the value is not a positive finite number.
The editor test reads the same class and compares with the value parsed from the config file it was loaded from: 62.5.

### Things to know for E4/E8

- **The simulation step is an integer number of milliseconds**: `FixedStepMS = (int32)(1000 / rate)`. At 62.5 Hz that is exactly
  16 ms, so frame time `f / 62.5` equals `f * 16 ms` and the ruler's millisecond line is exact. At 60 Hz the step is 16.67 ms
  real but **16 ms in the sim clock** (`FixedStepMS` truncated), so a project at a rate whose period is not a whole number
  of milliseconds drifts between "frame / rate" and the NP sim clock. The editor and the baker should keep using
  `frame / rate` (the real-time meaning, also what `FGSTN_TimelineValidation` assumes); the 62.5 Hz choice is what makes both agree.
  Worth a validator warning in E11 when `1000 / rate` is not an integer.
- The baker stores the rate it used (`FGSTN_BakeInfo::TickRateHz`, E8), so a change of the project rate marks bakes stale.
- Spike S2's "equal to the runtime value logged in PIE" half is not run (no PIE in this session): the runtime copies the CDO
  (`:50`, `:74-76`), so equality is by construction.

## S1 Montage sampling (automation test with a real RHI, 2026-10-23)

Test: `GSTNCombat.Editor.Preview.Spike.MontageSampling` (`Private/Tests/GSTN_EditorTests_PreviewSpike.cpp`, run with `-RenderOffscreen`, no editor window). It finds its
content through the Asset Registry (the first montage of the project with a root motion track and the first without, each with the preview mesh of its skeleton),
so it names no path: here the GASP mannequin (`SKM_UEFN_Mannequin`, 93 bones) with `M_interaction_bench_idle_loop_Montage` (25 s, root motion, blend-in 0.35 s) and
`M_LookAtPOI_test_relaxed_gesture_agree` (10.8 s, no root motion, blend-in 0.5 s). It skips with a warning in a project that has none.

### Method

`UDebugSkelMeshComponent` in an `FPreviewScene` (nothing ticks), `EnablePreview(true, Montage)`, `UAnimPreviewInstance::MontagePreview_PreviewNormal(INDEX_NONE, false)` and
`MontagePreview_RemoveBlendOut()` once, then for each frame `MontagePreview_JumpToPosition(Position)`, `TickAnimation(0, false)`, `RefreshBoneTransforms()` and
`GetSocketTransform(Name, RTS_Component)` of three bones (the last, the middle and the quarter bone of the skeleton: no name assumed). `Position = SectionStart + Frame / 62.5 x PlayRate`.
60 frames, forward (run 1), backward (run 2), forward again (run 3); each montage with the mesh's post process anim instance on and off.

### Result

| Question | Answer |
|---|---|
| Can the pose be set per frame with no real-time tick? | **Yes.** `MontagePreview_JumpToPosition` then a zero-delta tick and refresh evaluates the montage at that position; nothing else runs. |
| Repeatable | run 1 vs run 3: **0 cm** (bit-identical) on both montages, with and without the post process instance. |
| Equal backwards | run 1 vs run 2 (frames 59 down to 0): **0 cm**. |
| Equal to the raw animation | Compared with the raw sequence under the montage (`UAnimSequence::GetBoneTransform` per bone up the parent chain, no anim instance): **2.7e-5 cm** (idle loop, root motion) and **6.2e-5 cm** (the bench "into" montage tried first, 450 cm of root travel), over 180 samples, when the root bone's own track is left out. With the root bone's track included the error is 20 cm and 450 cm: that is the root motion. The second montage (a gesture) disagreed with the same comparison by 91.8 cm either way; it is not additive and I did not chase it (repeatability and the backward check hold on it): see open point 1. |
| Root motion absent from the pose | **Yes.** Bone 0 (`root`) stays at the component origin over all 60 frames on every run; its animated track, which carries the root motion, is not in the pose. Toggling `SetRootMotionMode(NoRootMotionExtraction)` on the preview instance did not change this (bone 0 travel 0 cm both ways), so the preview montage is evaluated with the root locked whatever that mode says. The runtime sees the same thing only if the actor consumes the root motion and the mesh pose has it removed (Mover does, M3); for hitboxes in root space this is what is wanted. |
| Blend-in and notifies do not move the pose | **Yes.** The preview montage is at full weight at every position (frame 0 equals the raw sequence at time 0 although the montages blend in over 0.35 and 0.5 s); `SetPosition` without notifies fires none. |
| Post process anim instance (GASP) | `GetPostProcessInstance()` is non-null with the graph on and off; the sampled bones differ by **1e-13 cm** between the two. The graph is either not producing a different pose for those bones or not running on the preview instance: not distinguished. It did not break repeatability. |
| Additive layer | Not tested (neither sample montage is additive). |
| Time per bake of 60 frames x 3 sockets | **0.9 to 1.6 ms** on the idle loop (the first call of a process has warm-up, 3 to 7 ms); 7 to 8 ms on the gesture montage. Hundreds of times under any budget: a bake of 12 sockets x 120 frames would be about 25 to 130 ms. |

### Verdict and consequences

PASS on every stop condition of plan E0: the pose does not depend on a ticked graph, root motion is not in it, so the bake of S §7.2 stays as written (no
raw-sequence second code path) and E6 goes ahead. `FGSTN_PreviewPose::Evaluate(Frame)` is these calls (`Private/Preview/GSTN_PreviewPose.cpp`).

Things E8 must know:

- `UDebugSkelMeshComponent` **resets its own relative transform** when it changes what it previews (root motion processing, `EnablePreview` and
  `ResetRootTransform`), which silently dropped the mesh's placement in the scene (found by looking at the first snapshot: the mesh stood at the world origin with no
  turn). The scene therefore hangs the debug component from a plain scene component that carries the mesh-to-root placement. The baker's "component space to root space" is that
  placement (`FGSTN_PreviewPose::GetSocketRootSpace`).
- The engine's **orbit camera** reads the stored view rotation through its own convention (a 90 degree yaw offset, pitch as roll): a direction set on it is not the direction seen.
  The viewport uses a plain camera. Not S1, but found in the same session.

### Open points

1. The gesture montage's 91.8 cm disagreement with the raw sequence is unexplained (segment properties, rate scale or translation retargeting of a bone are the candidates). It does not affect repeatability; E8's
   `FrameAtFEqualsPoseAtFOverRate` test should run on a montage that matches the raw comparison (the synthetic montage of the `Preview.*` tests does, within 0.05 cm).
2. Whether the runtime's `PlaySyncedMontage` (M3-C10) puts the montage at the same position at frame 0 (blend-in, section start, play rate): the preview assumes position = section start at frame 0,
   weight 1. To confirm at E8 as planned.
3. The post process graph question above, if a hitbox ever depends on a bone the graph moves (foot IK). Not an issue for hand and weapon sockets.
