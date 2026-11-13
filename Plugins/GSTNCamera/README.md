# GSTNCamera

A presentation-only camera for Unreal Engine 5.8: a stack of camera modes blended in a gravity-relative frame (far platformer
follow, a free camera for areas no designer guides, over-the-shoulder aim with a left/right swap, a Tekken-style duel framing for two players and a Power Stone style shared framing for three or four, designer sections), with
manual orbit control, occlusion and a small set of local events. Computed locally on every machine for its own local players; never
simulated, never replicated, never created on a dedicated server.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Design: [camera.md](../../docs/architecture/camera.md).
Binding rules: [modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.7. Differences from the spec:
[camera-doc-deltas.md](../../docs/notes/camera-doc-deltas.md).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCameraCore` | exists (Runtime) | Pure types and `FGSTN_CameraMath`: gravity frame (reference forward, heading, pitch, parallel transport), rate-limited Up, blend and the mode stack that keeps the weight of a reversed blend, orbit clamp and recenter, rail projection, section priority, rule choice with delays and dwell, converged aim, frame-change test, free camera ground height, duel framing, the cross-over safe duel step, shoulder swap blend, arena target choice and framing, group framing choice, occlusion decision. Native `Camera.*` tags. No world, no Engine. Covered by `GSTNCamera.Core.*`. |
| `GSTNCameraRuntime` | exists (Runtime) | Settings, `UGSTN_CameraComponent`, the local-player subsystem (orbit, frame, options, frame event), the section actor and registry, the modes and the rule table, subject adapters (actor, character), occlusion (fade, pull-in), converged aim and crosshair data, console variables, the Blueprint wrapper `UGSTN_CameraMath`. Covered by `GSTNCamera.Runtime.*`. |
| `GSTNCameraEditor` | not built | Section visualiser, validators (spec §8). |
| `GSTNCameraMover`, `GSTNCameraInput`, `GSTNCameraDepthMarker` | not built | Adapter and bridge plugins of spec §8. |

## Dependencies

- Plugins: `EnhancedInput` (engine).
- `GSTNCameraCore` (`Build.cs`): Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction or Mover.
- `GSTNCameraRuntime`: Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, InputCore, EnhancedInput, GSTNCameraCore. A Mover
  adapter would be a separate plugin; the runtime knows no movement type besides `UCharacterMovementComponent` and plain actors.

## Using it

1. Enable the plugin (`EnabledByDefault` is false).
2. Add `UGSTN_CameraComponent` to the pawn the local player follows (pawn data, a Game Feature `AddComponents` action or Blueprint) and
   make it the view target camera. It is not replicated and inert on a dedicated server. **Nothing in the project does this yet**:
   no pawn or Blueprint was changed.
3. Tune Project Settings > Plugins > GSTN Camera. The plugin ships no content: set the look action, the recenter action and the
   mapping context (soft references) to get plugin look input, or call `AddLookInput` from your own input code.
4. Let the pawn tell the camera about itself (all optional): `IGSTN_CameraSubject` (presented pivot, Up, suppressed), `IGSTN_CameraTagSource`
   (the tags the rule table reads; implement it from the ability system), `IGSTN_CameraAimOrigin` (where shots leave from). Without them an
   adapter reads the actor (a character reads its movement component gravity) and the tag container is empty, which yields Follow.

## Modes

| Tag | Role |
|---|---|
| `Camera.Mode.Follow` | Far follow (650 cm arm, -25 degrees, FOV 65), damped pivot, free manual orbit, never turns with the character. The built-in default. |
| `Camera.Mode.Free` | Follow plus the Astro Bot reading of spec §2.4: the pivot height follows the last ground not the jump arc, look-ahead, pitch help near drops, recentre action. Select it in the rule set (or `DefaultModeTag`) for Expedition and free areas. |
| `Camera.Mode.Shoulder` | Arm 220 cm, shoulder and Up offsets, FOV 55; look input becomes aim input (scale 0.6); pull-in occlusion; converged aim and crosshair data. The side is `Shoulder.ShoulderSide` (Right by default); `SetShoulderSide`, `ToggleShoulderSide` (BlueprintCallable) or the optional `SwapShoulderAction` override it per player, and the camera slides across in `ShoulderSwapBlendTime` (0.2 s), never snapping. The converged aim is computed from the resulting pose, so it is correct on both sides. |
| `Camera.Mode.Section` | The active section: `Orbit` (far follow at the designer angle), `FixedPoint`, `Rail`. |
| `Camera.Mode.Duel` | Exactly one opponent: midpoint of the local pawn and the opponent, looking perpendicular to the line between them projected on the plane normal to Up (both fighters parallel to the screen), arm from the separation. A jump over the opponent does not spin the view: the camera keeps its side of the line and the yaw never exceeds `DuelMaxYawRateDegPerSec` (90); the side only swaps after the fighters stayed crossed `DuelSideSwapDelay` (0.5 s) with both grounded, or `DuelSideSwapMaxDelay` (3 s) regardless. Closer than `DuelMinSeparation` (150 cm, e.g. one right above the other) the axis is frozen. |
| `Camera.Mode.Arena` | 2 or more opponents (3-4 players), Power Stone style shared framing: everybody (the local pawn and the registered opponents) in one elevated 3/4 view. The yaw is arena relative and never turns with a player: the facing of the arena section when the mode comes from a section (its actor forward about Up), else the reference forward of the gravity frame, plus `YawDeg` (45). The look-at point is a weighted centroid of the fighters (`LocalPlayerWeight` 1.5), damped (`FocusDampingSeconds`) and clamped to the section volume when there is one. The distance is the smallest within `MinArmCm`/`MaxArmCm` that keeps a sphere of `FighterRadiusCm` around every fighter inside the screen with `ScreenMargin`, damped with separate speeds (`ZoomInSeconds` 0.8 slow, `ZoomOutSeconds` 0.2 fast); the pitch rises with the distance (`PitchDeg` -35 plus up to `MaxExtraPitchDeg` more). All of it in the local Up frame, so any gravity works. Pure maths: `FGSTN_CameraMath::ArenaFrameAll`, `ArenaPitchForArm`, `StepArenaZoom`. No target selection. |

A designer adds a mode with a Blueprint child of `UGSTN_CameraMode`, a `UGSTN_CameraModeDefinition` and a `Camera.Mode.<Name>` tag; a rule
names the definition. The built-in rule table is: has `Camera.Request.Aim` and not `Camera.State.ShoulderBlocked` -> Shoulder (100);
`Camera.State.OneOpponent` -> Duel (90); `Camera.State.ManyOpponents` -> Arena (90); `Camera.State.InSection` -> the section mode (50); else Follow (0).
The opponent tags are computed locally from the actors given to `SetOpponents` (`SetOpponent` stays as the one-entry form). Replace it with a `UGSTN_CameraRuleSet` (settings, component or
section). Chosen rules are debounced (`ExitDelaySeconds` 0.15, `MinDwellSeconds` 0.3), so a predicted tag that flickers never shows.

## Sections and manual control

`AGSTN_CameraSection` is a volume with a `Spec`: identity tag, priority, rule set override, shoulder policy, path, desired yaw/pitch/distance/FOV,
`bAllowManualControl` with limits around the desired angle, recenter delay, blends, cut on enter, stick hold, presentation actions. Membership is a
point-in-volume test of the presented pivot each frame (never an overlap event). Overlaps: highest priority, then entered last, then the smaller
volume. A volume without brush geometry (made in code) uses `FallbackExtent`. Outside a section the orbit is free (D3); entering a section discards
the manual offset; leaving one keeps the current angle as the free orbit. With manual control off the look input is consumed.

`OnReferenceFrameChanged` (component, local-player subsystem, plus a native delegate) is raised when the camera itself turns the stick reference by at
least `HoldMinYawDeltaDeg`; manual orbit never raises it. The hold itself lives in the input source and is not part of this pass.

## Occlusion

One sphere sweep per camera per frame on `TraceChannel`. `FadeThenPullIn` (default for Follow, Free, Section, Duel) fades blockers tagged
`GSTN.CameraFade` (component or actor) through custom primitive data slot `FadeCustomDataIndex` and pulls in only for hard blockers within
`NearClipSafeCm` of the camera; Shoulder pulls in (fast in, slow out). The fade material function is not shipped.

## Gameplay tags

Native, owned by this plugin (spec §9): `Camera` (root), `Camera.Mode.{Follow,Free,Shoulder,Section,Duel,Arena}`, `Camera.Request.{Aim,Scope}`, `Camera.Section`
(root), `Camera.State.{InSection,ShoulderBlocked,OneOpponent,ManyOpponents}`. The `State` tags are computed locally and never on an ability system. Designer leaves
(`Camera.Mode.<Name>`, `Camera.Request.Scope.<Name>`, `Camera.Section.<Name>`) are ini content of the project that adds them; the plugin ships no tag ini.
The simulation never grants or reads any of them.

## Console variables

`gstn.Camera.Enable` (0 = off on this machine), `gstn.Camera.Debug` (pivot, line, mode, section, stack), `gstn.Camera.Occlusion` (0 = no probe).

## Tests

`GSTNCamera.Core.*` (27): blend monotonic and shortest arc, orbit clamp, recenter, Up rate limit and 180 degree flip, parallel transport, heading under
sideways gravity, section priority and ties, rule priority and exit debounce, reversed blend keeps its weight, rail projection, converged aim, frame
change ignores manual orbit, free ground height, duel framing, occlusion decision. Every frame-dependent test runs for flipped, sideways and diagonal Up.
`GSTNCamera.Runtime.*` (17, headless): stack push/pop/blend, per-entry mode state, section registry, missing tag source falls back to Follow, dedicated
server creates nothing, component not replicated, subject lost holds then cuts, two players independent, fade versus pull-in, look consumed when manual
control is off, section priority and shoulder policy, frame event on a section cut, duel and free modes, tags and library, shoulder side swap, rules choose Duel or Arena by opponent count, arena frames everybody with a stable yaw.
The new Core tests (`DuelCrossOverDoesNotSpin`, `DuelDegenerateAxisFrozen`, `DuelSideSwapsAfterDelay`, `ShoulderSwapBlendsAndAimStaysConverged`, `GroupFramingByCount`, `ArenaFramesAllFighters`, `ArenaYawStableWhenPlayersCircle`, `ArenaZoomDamped`, `ArenaFocusClampedToBounds`) and the three new Runtime ones (`ArenaFramesEveryoneWithStableYaw` replaces the target switch one) were written without being run: the editor was open on the owner machine, so only the build was checked.

## Next steps (not built)

The Editor module (section visualiser, validators), the scope zoom overlay (D7, spec §6.1), the crosshair presenter widget (the component only publishes
`OnCrosshairUpdated` and `GetAimDirection`), `GSTNCameraMover`, `GSTNCameraInput` (stick hold and the aim rotation provider), `GSTNCameraDepthMarker`,
the test map `L_Camera_Min`, the harness scenarios, spikes C1-C6, split screen and the arena bounds volume of the duel. Open points: the duel side rule is to be judged in play (CAM.8); the arena framing is a first pass (no Smash-like zoom on launches; the focus limit is the AABB of the arena section volume, coarse under a tilted Up; the damped zoom out can let a fast fighter touch the margin for a moment); four or more opponents also get Arena; when the group does not fit at `MaxArmCm` the framing is best effort.

## Known limits (v0.1)

- `LookInputSource = ControlRotation` only turns the plugin look input off; it does not read the controller back into the orbit.
- The look action is polled from the player's Enhanced Input; units are degrees per input unit per frame (a mouse delta). Give a stick action a Scale
  by Delta Time modifier.
- `FixedPoint` and `Rail` always look at the pawn; the yaw/pitch look-at limits of the spec are not built.
- A section cannot change the occlusion policy yet.
- Never run against a real pawn yet: only the headless tests exercised it. Spikes C1 (tick order against Network Prediction interpolation) and C4
  (control rotation write-back against the sample look code) are open.
