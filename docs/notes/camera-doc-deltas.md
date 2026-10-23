# Camera: deltas between docs/architecture/camera.md and the code (camera 1/2 and 2/2)

Status: 2026-10-23, written with the implementation of `Plugins/GSTNCamera`. The spec was not edited; this file lists what the owner should
fold into it or reject. Owner questions CAM.1-CAM.11 are unanswered: the *Proposal* defaults were used (list at the end).

## Differences

1. **`UGSTN_CameraMath` split.** The spec puts it in `GSTNCameraCore`, but a `UBlueprintFunctionLibrary` is an Engine class and the core has no
   Engine dependency. Core has `FGSTN_CameraMath` (plain static struct, all the tested maths); Runtime has `UGSTN_CameraMath`, a thin Blueprint
   wrapper (as `GSTNDepthMarker` does). Spec §8, §10 should say so.
2. **A fifth mode, `Camera.Mode.Free`.** Spec §2.3 lists Follow, Shoulder, Section, Duel and puts the Astro Bot behaviour (§2.4) inside Follow
   tuning. Code has `Follow` (the plain far follow) and `Free` (Follow plus ground-height follow, look-ahead, pitch help, recentre action),
   because the owner asked for D5 as a separate camera ("quando non siamo in un livello dove il designer guida la telecamera"). The built-in rule
   table still ends in Follow; `Settings.DefaultModeTag` or a rule selects Free. New native tag `Camera.Mode.Free`.
3. **No plugin content.** `Plugins/*/Content/` is gitignored, so the spec's default rule set, look action and mapping context are not assets. The
   default rule set is built in code (`UGSTN_CameraRuleSet::MakeDefault`), the input assets are empty soft references in the settings.
4. **Tags: native only, no ini shipped.** Spec §9 allows a plugin tag ini; none is needed for the native leaves, so `GSTNCameraCore` registers no
   `Config/Tags` folder. Designer leaves belong to the project that adds them.
5. **Occlusion as data, not classes.** Spec §10 has `UGSTN_CameraOcclusionPolicy` (+ `_PullIn`, `_Fade`, `_FadeThenPullIn`) classes. Code has the enum
   `EGSTN_CameraOcclusionPolicy` and `FGSTN_CameraOcclusionTuning` on the mode definition and settings, with the decision as a pure function
   (`DecideOcclusion`). A section cannot override the policy yet. Fade writes a custom primitive data float (settings slot); the material function is not shipped.
6. **Look input is polled.** The component reads the Enhanced Input action values from the player `UEnhancedPlayerInput` each frame (no input component
   to bind), adds the mapping context once through the local-player subsystem, and also exposes `AddLookInput`. `LookInputSource = ControlRotation`
   only switches the plugin input off (no read-back of the controller into the orbit).
7. **Component API beyond the spec.** `SetRequestTags` (tags fed without `IGSTN_CameraTagSource`), `SetSubjectOverride` (project glue for the §7.3 derived
   transform), `SetOpponent` (Duel), `Cut`, `RequestRecenter`, `OnReferenceFrameChangedNative`, `OnCrosshairUpdated`, `GetAimDirection`.
8. **Stack entries are (definition, section).** Moving between two adjacent sections therefore blends between them instead of jumping. Not in the spec.
9. **`FixedPoint` and `Rail` limits.** The spec has `LookAtYaw/PitchLimitDeg`; not built, they always look at the pawn. The spec field `RailOffset` is
   `CameraOffset` in code and also applies to `FixedPoint`. `FocalOffset` is in the section actor space.
10. **Stick hold flags.** Section `StickHold` and `HoldMinYawDeltaDeg` decide the frame event for section entries and exits; a non-section mode change uses
   "any change" and respawn/teleport always raises (`bCut`). The hold itself (`IGSTN_ReferenceFrameHold`, `FGSTN_ReferenceFrameHold`) and its
   `GSTNInput.Core.HoldKeepsWorldDirectionAcrossGravityChange` test are not part of this pass.
11. **Settings and console variables.** `gstn.Camera.Enable`, `.Debug`, `.Occlusion` exist; `.Stats` and `STATGROUP_GSTNCamera` do not. `UMG` is not a runtime dependency
   (no widget presenter yet); `InputCore` and `EnhancedInput` are.
12. **`KOHoldSeconds`.** 0 = hold until the subject returns (spec default); above 0 the component stops overriding the view after that time (the engine
   camera component view takes over) until the subject returns, which is then a cut.
13. **Test names.** `DedicatedServerCreatesNothing` tests the pure rules (`ShouldCreateFor`, `ShouldRunFor`), not a dedicated world. Added tests:
   `FreeCameraGroundHeight`, `DuelFramingKeepsFightersParallelToScreen`, `OcclusionDecision`, `SectionPriorityAndShoulderPolicy`,
   `FrameChangeRaisedOnSectionCutNotOnManualOrbit`, `DuelAndFreeModes`, `BuiltinTagsAndMathLibrary`. `StackPushPopBlend` etc. use a throwaway world and a
   subject state fed by hand.
14. **Duel.** Vertical extent term, arena clamp (`AGSTN_CameraBoundsVolume`), Smash-like zoom on launches and the 3-4 player wide shot are not built (CAM.7).
15. **Collateral fix.** `GSTNDepthMarkerRuntime` tests named a constant `Flags` that collided with `IConsoleManager.h` (C4459 as error) once the unity
   bundle changed; renamed to `RuntimeTestFlags`.

## Defaults taken for the owner questions (all *Proposal* values)

| Question | Default used |
|---|---|
| CAM.1 automatic return to the designer angle | Off: `RecenterDelaySeconds` 0 = never; per section opt-in |
| CAM.2 which shoulder | Right (`bRightShoulder`), swap not built |
| CAM.3 hold or toggle aim | Not the camera business: it follows the `Camera.Request.Aim` tag |
| CAM.4 stick reference release | Not built (input side); release on neutral only is the planned default |
| CAM.5 per-player or shared camera, split screen | Per `ULocalPlayer`, nothing shared, split screen not exercised |
| CAM.6 fade or pull-in | Fade then pull-in for far modes, plain pull-in for Shoulder |
| CAM.7 PvP with 3-4 players | Local pawn and one opponent (`SetOpponent`); no wide shot |
| CAM.8 own side of the screen online | Local player on the left (`bLocalPlayerOnLeft`) |
| CAM.9 Astro Bot reading | The §2.4 table as proposed (`Camera.Mode.Free`) |
| CAM.10 overlay follows the camera | Spec text kept; overlay not built |
| CAM.11 movement while scoped | The ability and weapon decide; overlay not built |
