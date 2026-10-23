# Camera — far platformer camera, shoulder aim, designer sections (specification)

Status: **draft for owner review, 2026-10-23**. Presentation-only specification: the camera is never simulated and never replicated. It
feeds the simulation only through two recorded inputs, `CameraYaw` and `AimYaw/AimPitch`, which the input layer samples (§1, §5).
Owner direction that wins over this file: the answer to GM.6 quoted below; [game-design-direction.md](game-design-direction.md) §2d (gravity can
change arbitrarily), §6b (listen server). The four *Decisions* from the answer are in §0; **everything else is *Proposal*.**

Scope: the camera component and mode stack, the built-in modes, how a mode is chosen, designer sections, manual control, the contract with the input
layer (stick reference across cuts, aim), occlusion, hard cases, modules, tags, tests. Out of scope: cutscenes, replay or spectator cameras, a 2.5D plane
lock (not asked; a simulation change, game-modes §5), a shared camera framing all players (open, CAM.5), HUD beyond the crosshair presenter.

Sources: [game-modes.md](game-modes.md) §1, §5 ("Camera sections"), §8.2 (`Camera.*`), §9.3; [combat.md](combat.md) §6.1 (`CameraYaw`), §11.6 (camera aim);
[input.md](input.md) §4, §5 (`bSendCameraAim`, input sources); [depth-marker.md](depth-marker.md) §6.5 (`PushStyleOverride`); [modularity-and-tags.md](modularity-and-tags.md)
§3.2, §3.6–§3.10, §4.1–§4.2b; [combat-m6-defence-links.md](combat-m6-defence-links.md) §7.3 (derived transform as camera target); [combat-m7-piggyback.md](combat-m7-piggyback.md) §2, §5 (camera anchor).

Notation: *Decision* = decided by the owner or elsewhere. *Proposal* = this document's suggestion. *Unsure* = I could not verify it against engine
source, Lyra or the project in this session (I did not inspect the project's current camera). "Estimate" = a number to measure, not a fact.

Owner answer to GM.6 (2026-10-23, verbatim): "La telecamera dovrebbe essere in generale abbastanza lontana dal giocatore, stile mario 3d world. E'
possibile pero che vada sulle spalle per permettere di mirare con alcune armi e alcune abilità. Dovrebbe essere possibile muoverla manualmente, ma
durante i livelli il designer puo specificare l'angolo desiderato, e se si puo spostare o no."

## 0. Summary

- *Decisions* from the answer: **D1** the default camera is far from the player, Mario 3D World style. **D2** it can go over the shoulder to aim,
  only with some weapons and abilities. **D3** the player can move it by hand. **D4** in levels the designer sets the desired angle and whether it can be moved.
- Plugin **`GSTNCamera`**: one `UGSTN_CameraComponent` on the pawn runs a **stack of camera modes** with blending (the Lyra idea, §2). Built in:
  **Follow** (far, default), **Shoulder** (aim), **Section** (designer angle, fixed point or rail). Modes are data assets plus Blueprintable classes.
- **The mode is chosen by data** (§3): a rule table (tag query to mode, priority) over the pawn's tags, read through a small interface so the plugin knows no GAS,
  plus the active camera section. "Some weapons and abilities" = the ability or weapon grants `Camera.Request.Aim`; no code names a weapon.
- **Sections** (§4): `AGSTN_CameraSection` volumes with a spec (angle, distance, FOV, manual control or not, limits, recenter, blends, priority); outside them, free orbit.
- **Input contract** (§5): the camera owns the control rotation on the owner; the input source samples `CameraYaw` and aim from it; at a camera cut it
  **holds the stick's reference direction until the stick returns to neutral**. The hold lives in the input source, not in the simulation.
- **Everything is relative to gravity-up and rate limited** (§7): no snap on a gravity change. Far-camera occlusion defaults to **fading the occluder**, pull-in as fallback.
- **Local per player**: nothing replicates, the host's camera is a client camera (§1, §7). No dependency on combat, GAS, NP, Mover (outside an adapter), the depth marker or the project (§8). Root `Camera.*` owned by `GSTNCamera` (§9).

## 1. Scope and the boundary with the simulation

| Direction | What | Rule |
|---|---|---|
| Camera to simulation | `CameraYaw` (stick reference, combat.md §6.1) and `AimYaw/AimPitch` (§6); the aim origin stays the character (combat.md §11.6), the camera position is never simulated | **Only through the input source**, sampled once per local input sample and stored in `FGSTN_InputCmd`. A rollback re-simulates from the stored commands, so the camera is never re-run and cannot cause a reconcile. The camera never writes sim state, activates an ability or queues a Mover move |
| Simulation to camera | Presented pivot, Up, suppressed flag, the ASC tags used by the rule table | Read from the **presented** actor (owner: the predicted actor; others: the interpolated one) through `IGSTN_CameraSubject` and `IGSTN_CameraTagSource` (§3, §8). CLAUDE.md rule 1 (no component queries) binds simulation code only |
| Network | nothing | No replicated property, RPC, sync or aux state. Every machine, host included, runs its own camera for its own local players |

No `*NP`, ability or hit module includes a camera header, so nothing in the simulation can read the camera: rollback-safe by construction.

## 2. The camera mode stack

### 2.1 The idea, and what is borrowed

Lyra, from memory (*Unsure*: class names and semantics against current Lyra and 5.8; no Lyra source in this session): a camera component on the pawn
overrides the camera view; a delegate asks the pawn for the **desired camera mode class** each frame (pawn data, or an active ability); the component keeps a
**stack of mode instances**, pushes the desired one when it differs from the top, evaluates every entry into a view (pivot, rotation, FOV), **blends** them by
per-entry weights (blend time and function on the mode) and drops covered entries. We borrow the structure, not the code, and change two things: the frame is
**gravity-relative** (§7), and the choice is a **data table over tags** (§3), not a delegate wired in Blueprint. Alternative: Epic's Gameplay Cameras plugin
(*Unsure*: its state in 5.8, not evaluated); spike C2 compares, a bridge could drive it later.

### 2.2 Evaluation, once per game frame per local player (after the pawn's presentation update, spike C1)

1. Build the **context**: presented subject state (§8), `Up`, active section spec, tags, orbit state, delta time (real time, never combat frames).
2. **Select** the desired mode (§3). If it differs from the top entry: push it with the definition's blend; if it is already in the stack, move it to the top
   **keeping its weight**, so a reversed request reverses the blend from where it is, not from zero.
3. Each entry computes its `FGSTN_CameraView` (pivot, heading and pitch about `Up`, arm length, offsets, FOV).
4. **Blend** from the bottom: heading by shortest arc; pitch, arm, FOV, pivot and offsets linear; all in the gravity frame, never as world quaternions
   (avoids roll flips). Entries below a weight of 1 stay, covered ones drop; depth capped (`MaxStackDepth`, 4).
5. Post: gravity-frame smoothing and occlusion (§7), fill `FMinimalViewInfo`, **write back** the control rotation (§5), publish aim and crosshair data (§6).

### 2.3 Built-in modes

| Mode (identity tag) | Role | Defaults (*Proposal*, estimates, §10) | Notes |
|---|---|---|---|
| **Follow** `Camera.Mode.Follow` | The default. Far, Mario 3D World style (D1) | Arm 650 cm, pitch -25 degrees, FOV 65, pivot damping 0.25 s, free manual orbit (D3), no auto-align to movement | Never inherits the character's rotation. Occlusion: fade (§7.1) |
| **Shoulder** `Camera.Mode.Shoulder` | Over the shoulder to aim (D2) | Arm 220 cm, offset 55 cm to the shoulder side and 55 cm along Up, FOV 55, aim sensitivity 0.6 | Camera input becomes aim input; crosshair; pull-in occlusion (§6) |
| **Section** `Camera.Mode.Section` | The designer's angle (D4) | Everything from the active section spec (§4) | Three paths: `Orbit` (far follow at a designer angle), `FixedPoint`, `Rail` |

**Modes are data.** `UGSTN_CameraModeDefinition` (PrimaryDataAsset): identity tag (exactly one `Camera.Mode.*` leaf), an instanced `UGSTN_CameraMode`, blend in and out (`Camera|Blend`),
crosshair on or off, default manual-control flag, look sensitivity, occlusion policy. `UGSTN_CameraMode` is `Abstract, Blueprintable, EditInlineNew`: `BlueprintNativeEvent
ComputeView(const FGSTN_CameraModeContext&) -> FGSTN_CameraView`, the C++ built-ins are the `_Implementation`, a Blueprint child may call the parent. A designer adds a mode with a
Blueprint child, a definition and a new `Camera.Mode.<Name>` ini leaf; replacing Follow is another definition under the same tag in the Experience data (override order, modularity §3.9).
Stack entries are runtime copies, so a mode may keep smoothing state; the asset stays read-only. Presentation only: Blueprint is free (no determinism contract), cost counts against §10.

## 3. Who picks the mode

### 3.1 The rule table

`UGSTN_CameraRuleSet` (DataAsset): `DefaultMode` and `Rules[]` of `FGSTN_CameraModeRule { FGameplayTagQuery Query; Mode (soft definition) or bModeFromSection; int32 Priority;
EnterDelaySeconds; ExitDelaySeconds }`. Highest priority whose query matches wins; ties go to table order. Default set in the plugin's `Content/`:

| Priority | Query over the tag container | Mode |
|---|---|---|
| 100 | has `Camera.Request.Aim` and not `Camera.State.ShoulderBlocked` | Shoulder |
| 50 | has `Camera.State.InSection` | the active section's mode (`bModeFromSection`) |
| 0 | empty (always) | Follow |

Which set applies, lowest to highest: plugin default < project settings < Experience or pawn data < the active section's own `RuleSetOverride`.

### 3.2 What the query reads

The container is the union of (a) the tags from **`IGSTN_CameraTagSource::GetCameraTags`** on the view target: a small interface the project's pawn (or one project component) implements
from its ASC, so `GSTNCamera` knows no GAS, and (b) **synthetic tags computed locally** each frame: `Camera.Section.<Name>` (the active section's identity), `Camera.State.InSection`,
`Camera.State.ShoulderBlocked` (the section forbids Shoulder), never granted to an ASC. A project reacts to its own tags (wall slide, tumble, carrying) by adding rules in data: the plugin
names none (modularity §3.7 rule 4). A pawn without the interface yields an empty container: Follow.

### 3.3 "Some weapons and some abilities" is data (D2)

The aiming ability, or a weapon's or kit's ability set, lists `Camera.Request.Aim` among the tags it owns while active (`ActivationOwnedTags` or a gameplay effect, as for any stance, combat.md §11).
A kit that never grants it never gets Shoulder; a new aimed weapon needs no camera change. The tag is **locally predicted on the owner**, so it exists in the frame the ability starts (no round
trip), which is fine for presentation. **The simulation never reads `Camera.*`**: a lint rule (modularity §4.8) rejects `Camera.*` in any ability's required or blocked tags.

### 3.4 Rollback, flicker, hysteresis

The tag lives in rolled-back ASC state and may flicker (predicted grant, correction removes it, re-simulation grants it again), or vanish for good when the server refuses the ability
(then the camera blends back after the exit delay, which is correct).

| Measure | Effect |
|---|---|
| Tags are read **once per game frame**, after any correction of that frame has run (*Unsure* of the fork's order, spike C1) | A resimulation inside one frame never shows |
| `ExitDelaySeconds` (default 0.15) and `MinDwellSeconds` (0.3) on the chosen rule | A one or two frame drop does not leave Shoulder; a real end still does |
| Reversible blends (§2.2 step 2) | A flip that comes back is a small wobble, not a restart |

A **remote** pawn's tags may not be visible on a non-owner proxy (combat.md: abilities do not run there, tags are projected; *Unsure*, as in depth-marker §6.4): spectating falls back to Follow.

## 4. Manual control and the designer's angle

### 4.1 Section spec (D4) and the section actor

`FGSTN_CameraSectionSpec`, inline on `AGSTN_CameraSection` (an `AVolume`; optional `USplineComponent` for rails). An optional `UGSTN_CameraSectionPreset` asset only feeds an editor "copy values" button.

| Group | Fields (UPROPERTY category Camera, then Section) |
|---|---|
| Identity | `SectionTag` (`Camera.Section.*`), `ModeTag` (default `Camera.Mode.Section`), `Priority`, `RuleSetOverride`, `ShoulderPolicy {Allow, Forbid}` |
| Angle, relative to Up | `Path {Orbit, FixedPoint, Rail}`; `DesiredYawDeg` (about Up, relative to the actor's forward projected on the plane normal to Up), `DesiredPitchDeg`, `DistanceCm`, `FovDeg`, `FocalOffset`; `UpSource {Gravity, SectionActor}` |
| Manual control (D4) | `bAllowManualControl`, `YawLimitDeg` and `PitchMinDeg/PitchMaxDeg` **around the desired angle**, `DistanceScaleLimit` (zoom, 1 = none) |
| Recenter | `RecenterDelaySeconds` (0 = never), `RecenterDegPerSecond`, `bOnlyWhileMoving` |
| Transition | `BlendInSeconds`, `BlendOutSeconds`, `BlendFunction`, `bCutOnEnter`, `StickHold {None, OnCut, OnAnyChange}`, `HoldMinYawDeltaDeg` (20) |
| Presentation | `Actions[]`: `UGSTN_CameraSectionAction` (Blueprintable, EditInlineNew): `OnSectionEntered/Exited(const FGSTN_CameraSectionEvent&)`, run locally |

Path meaning: `Orbit` = the far follow camera at the designer's yaw, pitch and distance. `FixedPoint` = the camera sits at the actor's transform (plus offset) and turns to look at the pawn within `LookAtYaw/PitchLimitDeg`. `Rail` = the
camera slides on the spline to the point nearest the pawn's presented pivot (`RailOffset`), looking at the pawn or along the tangent.

### 4.2 Which section is active

`AGSTN_CameraSection` registers with `UGSTN_CameraSectionSubsystem` (world subsystem, not created on a dedicated server) on **every** machine. Membership is a **point-in-volume test of the local view target's presented
pivot** each frame, never an overlap event (so remote pawns, corrections and rollbacks cannot trigger one). Overlaps: highest `Priority`; equal priority: the section entered last; still tied: the smaller volume. Leaving the last
section blends out over `BlendOutSeconds` **keeping the current angle as the free orbit**: no snap back to the mode's default yaw.

### 4.3 Manual control

Camera input (right stick, mouse) is handled by the plugin: `LookAction` (an `UInputAction`, soft default in plugin Content, replaceable, modularity §3.9) with a default mapping context added to the local player's Enhanced Input
subsystem; `LookInputSource {Plugin, ControlRotation}` lets a project that already feeds `AddControllerYawInput` keep it (*Unsure*: how the sample character feeds look today). The **orbit state** (`YawDeg`, `PitchDeg`, `DistanceScale`,
relative to the smoothed Up) lives in the local-player subsystem, so it survives a respawn and carries across mode changes (Follow to Shoulder keeps the heading: the aim does not jump).

| Situation | Behaviour |
|---|---|
| Outside any section | The mode's defaults, free orbit (D3), per-player sensitivity, invert and recenter options (user settings, not pawn data) |
| In a section, `bAllowManualControl` | Orbit clamped to the limits around the desired angle; with `RecenterDelaySeconds` > 0 it eases back to the desired angle after that long without input |
| In a section, not allowed | Look input is consumed by the plugin (not forwarded), the view stays at the desired angle. Shoulder still takes aim input unless `ShoulderPolicy = Forbid` |
| Entering a section | Blend (or cut) to the desired angle; the manual offset is **discarded** |
| Shoulder active | Look input is aim input; section limits do not apply; pitch limits are the Shoulder tuning's |

## 5. Contract with the input layer: control rotation, stick reference, latch

**Control rotation.** On the owning machine the camera component is the **only writer** of the player controller's control rotation (`ControlRotationPolicy = WriteBack`, default): it writes the full view rotation, including the
roll that keeps the camera's up on gravity-up. `UGSTN_PlayerInputSource` already samples `CameraYaw` and `AimYaw/AimPitch` from the control rotation (combat.md §6.1, §11.6); the input plugin needs no camera knowledge.
How `CameraYaw` is defined under a non-Z `Up` is *Unsure* in the input sim; §7.2 states what the camera needs.

**The stick across a camera change.** The stick is raw (`StickX/Y`, combat.md §6.1) and the simulation rotates it by `CameraYaw`. If the camera cuts or blends to another angle while the stick is held, the world direction would
turn at once and the player would run the wrong way. The rule (game-modes §5 already proposed it) is made precise as a contract:

1. **Event.** The camera raises `OnReferenceFrameChanged(FGSTN_CameraFrameChange { FromForward, ToForward, Reason, bCut })` (local delegate) when **it decides** to turn the reference by at least
   `HoldMinYawDeltaDeg` (plugin setting, 20, overridable per section): section enter or exit (per its `StickHold`), a mode change that turns the heading, a respawn or teleport cut. **Manual orbit never raises it.**
2. **Hold, in the input source.** `GSTNInputCore` declares `IGSTN_ReferenceFrameHold::HoldReferenceFrame(const FVector& WorldForward)` and a pure state machine `FGSTN_ReferenceFrameHold`; `UGSTN_PlayerInputSource`
   uses it. If the stick is beyond `NeutralThreshold`, it stores the **world forward** the last sample used, and from then on **encodes `CameraYaw` from the held direction** against the *current* Up, so the held
   world direction survives both a camera change and a gravity change. Aim fields are untouched (the camera still aims).
3. **Release.** The stick below `NeutralThreshold` for `NeutralFrames` (2); optionally a turn of the stick by more than `ReleaseAngleDeg` since the hold began (default off, CAM.4); safety timeout `MaxHoldSeconds` (default 0 = none). A second event while holding keeps the **original** held direction.
4. **Not simulation.** Evaluated at sampling time on the controlling machine only: the server sees an ordinary `CameraYaw` sequence (no wire change); a resimulation reuses stored commands and never runs the hold.
   **Wiring:** bridge plugin `GSTNCameraInput` (§8) binds the event to the interface on the controller's input source; without it a project binds the same two things in a few Blueprint nodes, or accepts the flip.

## 6. Aim

- **Shoulder** puts the pivot at `ShoulderOffset` (side and along Up; `bRightShoulder`, swappable by a camera request, CAM.2), shortens the arm, narrows the FOV and turns look input into aim. Hold or toggle is the **ability's** business (`bEndOnTriggerRelease`, combat.md §11); the camera only sees the tag (CAM.3).
- **`bSendCameraAim` must be on** in the input config of every kit that has a camera-aimed ability (`AimPolicy = Camera`). It is data on the input config; a validator in the combat editor module warns when a kit has `AimPolicy = Camera` and the config does not send aim. Without it Shoulder works visually but the aim never reaches the simulation.
- **Aim origin is the character** (combat.md §11.6), the camera is not. With a shoulder offset, "camera forward from the muzzle" lands about 55 cm beside the crosshair at every distance. *Proposal* (closes the *Open* of §11.6): **converged aim**, default on in Shoulder:
  trace from the view centre (`ConvergeMinCm` 300 to `ConvergeMaxCm` 6000), and send `AimYaw/AimPitch` as the direction from the character's aim origin (`IGSTN_CameraAimOrigin`, presented socket; default eye point) to the hit point; no hit means camera forward. The value is
  quantised into the command like any aim, so the simulation stays deterministic. It needs one input-side hook (an aim-rotation provider on the input source, wired by the same bridge): *tell the owner*, it edits `input.md`.
- **Crosshair** = a replaceable presenter: `UGSTN_CrosshairPresenter` (Abstract, Blueprintable, EditInlineNew), `OnCrosshairUpdated(const FGSTN_CrosshairView&)` with screen position, hit point, distance, `bConverged`. Default presenter adds a plain widget (soft class in settings) to the local player's screen; a CommonUI project replaces it. The crosshair shows where the shot goes (the converged point).

## 7. Hard cases

### 7.1 Occlusion with a far camera

| Policy | How | Verdict |
|---|---|---|
| Pull-in | Sphere probe pivot to camera, camera moves in | Standard for close cameras. With a 650 cm arm it hides the depth cues a platformer needs, jitters on thin geometry, and slams the camera onto the character in corridors |
| **Fade** | Blockers found by the probe fade out through a per-primitive parameter (dither or opacity) and fade back | **Default for Follow and far sections.** The camera does not move, so the player's depth reading and the depth marker stay stable |
| Fade then pull-in | Fade what is fadeable, pull in only for non-fadeable blockers within `NearClipSafeCm` | Follow's fallback. Shoulder (close camera) defaults to plain pull-in with fast-in, slow-out smoothing |

Fadeable = components or actors with the tag `GSTN.CameraFade` (a level designer marks walls and ceilings) using a plugin material function on a custom primitive data slot. Probe: one sphere trace per local player per frame on a project channel (`Camera` by default),
radius `ProbeRadiusCm`. A section may override the policy (an indoor section pulls in). *Unsure*: cost of dithered fade with Nanite and Lumen (spike C3).

### 7.2 Gravity, and what yaw means

The camera frame is `Up` (from the subject, presented) plus a reference forward. `Up` is **rate limited** (`UpRateLimitDegPerSecond` 90, with smoothing): a gravity change turns the frame over time, never snaps. The reference forward is **parallel-transported** (the previous heading is carried by the shortest rotation
from the old Up to the new one), so the view of the world turns continuously and manual yaw and pitch are relative to the moving frame. Under sideways gravity "yaw" is the heading about Up from the transported forward, "pitch" the elevation from the plane normal to Up. A stable convention is needed only where a number leaves the camera:
`CameraYaw` must be the heading about Up from a **deterministic** reference direction (*Proposal*: world X projected on the plane normal to Up, world Y when degenerate), one function shared by the sim and the encoder (*Unsure* about today's convention in input.md). The hold (§5) keeps a world direction, so a gravity change cannot flip the run direction.

### 7.3 Other cases

| Case | Behaviour | Why |
|---|---|---|
| Wall slide | Nothing built in; a project data rule on its wall tag may select a Follow variant. The camera never reads Mover modes | Keeps the plugin free of Mover and of project tags |
| Launch, tumble | Pivot is the **presented capsule point** (`IGSTN_CameraSubject::GetPresentedPivot`), never the mesh or a bone; no rotation from the character; a data rule on the project's tumble tag may pick a looser Follow (wider dead zone, less lag). Hitstop: real delta time, the presented pivot freezes and the camera settles, no shake in v1 | A spinning victim must not shake the camera; fast launches must not lose the player |
| Piggyback, grab, link | The subject returns the pawn's own **presentation transform** (the link's derived transform, M6 §7, M7 anchor), supplied by project glue; each player keeps their own camera and look control | Rider and held pawn follow the pair without owning its yaw. Touches project camera code: *tell the user* (CLAUDE.md, shared Blueprints) |
| KO, shattering, respawn | Subject `bSuppressed` or lost: the camera **holds** its last view (`KOHoldSeconds` cap, default until the next view target), orbit state kept. Respawn, possession or teleport: **cut** (blend 0) to the new pawn's mode, orbit from the section at its position else the kept one, and raise the frame event (§5) | game-modes §5: follow stops on KO; no streak across the level |
| Split screen | Optional, off by default. State is per `ULocalPlayer` (a `ULocalPlayerSubsystem`), sections tested per local view target, one occlusion probe per player | Architecture allows it; CAM.5 |
| Listen host | The host's camera is an ordinary local camera; the host's frame pays its cost (§10) | Local per machine |
| Late join | The first possessed pawn starts with a cut, orbit from its section or `DefaultYaw`; sections placed in the level exist already, those inside streamed modules (Expedition) register on load and unregister on unload | No history is needed |

## 8. Plugins, modules and dependencies

| Plugin / module | Type | Depends on | Contents |
|---|---|---|---|
| **`GSTNCamera`** / `GSTNCameraCore` | Runtime | `Core`, `CoreUObject`, `GameplayTags` | Pure types and `UGSTN_CameraMath` (§10): frame transport, blend, orbit clamp, recenter, rate-limited Up, rail projection, section priority, rule choice with delays, converged aim, frame-change test. No world, no Engine; tested alone |
| `GSTNCamera` / `GSTNCameraRuntime` | Runtime | Core module, `Engine`, `EnhancedInput`, `DeveloperSettings`, `GameplayTags`, `UMG` (default crosshair only; a split `GSTNCameraUI` module if that is a problem) | Component, local-player and section subsystems, modes, section actor, input handling, occlusion, crosshair presenter, interfaces, settings, console variables, Actor and Character adapters. Not created on a dedicated server |
| `GSTNCamera` / `GSTNCameraEditor` | Editor | Runtime, `UnrealEd` | Section visualiser (desired view frustum, yaw and pitch limit arcs, rail, "pilot viewport through this section"), validators (equal-priority overlaps, `Rail` without spline, desired angle outside limits, `bAllowManualControl` off with recenter set), mode preview |
| **`GSTNCameraMover`** (adapter plugin) | Runtime | `GSTNCamera`, `Mover` | Subject adapter: `GetUpDirection`, presented transform; a separate plugin so a project without Mover can use the main one (as `GSTNDepthMarkerMover`) |
| **`GSTNCameraInput`** (bridge) | Runtime | `GSTNCamera` (Runtime), `GSTNInput` (`GSTNInputCore`, interfaces only) | Binds the frame event to `IGSTN_ReferenceFrameHold` and, with converged aim, supplies the aim rotation |
| **`GSTNCameraDepthMarker`** (bridge) | Runtime | `GSTNCamera` (Runtime), `GSTNDepthMarker` (Runtime) | `UGSTN_CameraSectionAction_DepthMarkerStyle { Style, Priority }`: `PushStyleOverride(Section, Style, Priority)` on enter, `PopStyleOverride(Section)` on exit (depth-marker.md §6.5) |

Layer: optional presentation feature (L4 on L1), `EnabledByDefault: false` for all five plugins; each bridge depends on exactly the main plugin and one other, nothing depends on a bridge (modularity §3.2, §3.7 rule 3). The marker style on a section is a **generic Blueprintable
section action** implemented by the bridge, so `GSTNCamera` does not know the marker; a project with both plugins can also write the action in Blueprint.
**Must never depend on:** `GASP_Giustino2` and `/Game/...`; `GSTNCombat*`, `GSTNMission*`, `GSTNMode_*`, `GSTNMinion*`, `GSTNLink*`; `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction`; `Mover` outside the adapter; `GSTNInput*` and
`GSTNDepthMarker*` outside the two bridges; any Editor module from a runtime module. `check_modules.py` rules 5, 10, 11, 12 apply (soft references, no `ConstructorHelpers`); canary build for `GSTNCamera` alone (modularity §3.7 rule 7).

## 9. Gameplay tags

Root `Camera.*` is owned by `GSTNCamera` (one owner, modularity §4.1; answers the *owner open* of game-modes §8.2 and modularity §4.2). Never granted by the simulation, never read by it (§3.3).

| Subtree | Leaves | Source |
|---|---|---|
| `Camera.Mode.*` | native `Follow`, `Shoulder`, `Section`; designer modes `Camera.Mode.<Name>` | native + **ini** (the content owner's `Config/Tags/*.ini`) |
| `Camera.Request.*` | native `Aim`; designer requests (`Camera.Request.<Name>`: zoom, swap shoulder, cinematic) granted by content | native + ini |
| `Camera.Section.*` | designer section identities (`Camera.Section.BossRoom`), usable in rule queries | root native, leaves ini |
| `Camera.State.*` | `InSection`, `ShoulderBlocked`: **computed locally**, added to the query container, never on an ASC | native |

No `Message.Camera.*` in v1 (local delegates and section actions cover it; the message router is *Unsure* in 5.8). Each root leaf has a `DevComment` stating who grants it and that it is local (modularity §4.1 rule 4).

## 10. Types, tuning, budget

| Type | Module | Kind | Role |
|---|---|---|---|
| `FGSTN_CameraFrame`, `FGSTN_CameraView`, `FGSTN_CameraBlend`, `FGSTN_CameraOrbitState`; `UGSTN_CameraMath` | Core | USTRUCT; BlueprintFunctionLibrary | Up and reference forward; pivot, heading, pitch, arm, offsets, FOV; blend; orbit. Maths is pure and `BlueprintPure`; tests use diagonal `Up` vectors |
| `FGSTN_CameraFrameChange`, `FGSTN_CrosshairView` | Core | USTRUCT, BlueprintType | §5, §6 |
| `UGSTN_CameraSettings` | Runtime | UDeveloperSettings | Default rule set, look action and context, trace channel, `DefaultYaw`, `KOHoldSeconds`, `LookInputSource`, `ControlRotationPolicy`, caps; defaults in `Config/DefaultGSTNCamera.ini` pointing at plugin Content |
| `UGSTN_CameraModeDefinition`, `UGSTN_CameraMode` (+ `_Follow`, `_Shoulder`, `_Section`), `UGSTN_CameraRuleSet`, `UGSTN_CameraSectionPreset`; structs `FGSTN_CameraSectionSpec`, `FGSTN_CameraModeRule` | Runtime | data, Blueprintable; USTRUCT (they reference Runtime classes) | §2.3, §3.1, §4.1 |
| `UGSTN_CameraComponent` | Runtime | UCameraComponent subclass | The stack, view, control-rotation write-back, aim and crosshair data; **never replicated** (*Unsure*: component versus a camera-manager hook, spike C2) |
| `UGSTN_CameraLocalPlayerSubsystem`, `UGSTN_CameraSectionSubsystem`, `AGSTN_CameraSection`, `UGSTN_CameraSectionAction` | Runtime | subsystems, actor, Blueprintable | Per-player orbit, options, KO hold; section registry; §4 |
| `IGSTN_CameraSubject`, `IGSTN_CameraTagSource`, `IGSTN_CameraAimOrigin` | Runtime | UINTERFACE (BlueprintNativeEvent) | Presented pivot, Up, suppressed; tags; aim origin. Adapter registry as in the depth marker (Actor, Character, Mover) |
| `UGSTN_CameraOcclusionPolicy` (+ `_PullIn`, `_Fade`, `_FadeThenPullIn`), `UGSTN_CrosshairPresenter` | Runtime | Abstract, Blueprintable, EditInlineNew | §7.1, §6 |

Tuning structs (`EditAnywhere, BlueprintReadOnly`, `ClampMin`, `ForceUnits`; values are estimates): `FGSTN_CameraFollowTuning` (`Camera|Follow`: `DistanceCm` 650, `PitchDeg` -25, `FovDeg` 65, `PivotDampingSeconds` 0.25, `bAutoAlignToMovement` false);
`FGSTN_CameraShoulderTuning` (`Camera|Shoulder`: `ArmCm` 220, `ShoulderOffsetCm` 55, `FovDeg` 55, `AimSensitivityScale` 0.6, `bConvergeAim` true, `ConvergeMinCm` 300, `ConvergeMaxCm` 6000);
`FGSTN_CameraBlendTuning` (`Camera|Blend`: `BlendSeconds`, `Function`, `Exponent`); `FGSTN_CameraStabilityTuning` (`Camera|Stability`: `UpRateLimitDegPerSecond` 90, `ExitDelaySeconds` 0.15, `MinDwellSeconds` 0.3, `MaxStackDepth` 4); `FGSTN_CameraOcclusionTuning`
(`Camera|Occlusion`: `Policy`, `ProbeRadiusCm` 25, `FadeSeconds` 0.15, `FadedOpacity` 0.2, `PullInSecondsIn` 0.05, `PullInSecondsOut` 0.6, `NearClipSafeCm` 50). **Budget** (estimate, to measure): at most 0.15 ms mean game thread per local player, one probe plus at most one aim trace
per frame; stat group `STATGROUP_GSTNCamera`; console `gstn.Camera.Debug`, `.Stats`, `.Occlusion`.

## 11. Testing

**Automation, pure (`GSTNCamera.Core.*`):** `BlendWeightsMonotonic`, `BlendHeadingShortestArc`, `OrbitClampedAroundDesired`, `RecenterReachesDesired`, `UpRateLimited`, `UpFlip180NeverSnaps`, `TransportedForwardContinuous`, `HeadingUnderSidewaysGravity`, `SectionPriorityHighestWins`,
`SectionTieEnteredLastThenSmaller`, `RuleHighestPriorityWins`, `RuleExitDelayDebounces`, `ReversedBlendKeepsWeight`, `RailProjectsToNearest`, `ConvergedAimBeyondMaxUsesForward`, `ConvergedAimMinDistance`, `FrameChangeIgnoresManualOrbit`; and in `GSTNInput.Core.*`
`HoldKeepsWorldDirectionAcrossGravityChange` (pure `FGSTN_ReferenceFrameHold`). Several `Up` vectors, diagonal ones included, so a hidden world-Z assumption fails.
**Runtime (`GSTNCamera.Runtime.*`):** `StackPushPopBlend`, `ModeStateIsPerEntry`, `SectionRegistryUnloadRemoves`, `MissingTagSourceFallsBackToFollow`, `DedicatedServerCreatesNothing`, `ComponentNotReplicated`, `SubjectLostHoldsThenCuts`, `TwoLocalPlayersIndependent`, `FadeVersusPullIn`, `LookConsumedWhenManualOff`.
**Test map `L_Camera_Min`** (plugin content, one plugin-owned pawn Blueprint): open field Follow; a corridor with a fixed `Orbit` section and a `Rail`; overlapping sections with different priorities; sections with manual control off and on with recenter; a shoulder-aim dummy at several distances;
sideways and flipped gravity volumes; fadeable and non-fadeable walls; a KO ravine with respawn; a lift.
**Harness (listen host plus 2 clients, then dedicated plus 2 clients):** `scenario_camera_replication` (**zero** bytes attributable to the plugin; host and clients keep different views), `scenario_camera_hold` (stick held forward across a section cut: the velocity direction stays within a few degrees
until neutral; with 2 % loss and forced rollbacks the cut adds **no** reconcile), `scenario_camera_aim` (with `bSendCameraAim` on, `AimYaw/AimPitch` reach the simulation and match the converged direction within quantisation).
**Spikes:** C1 tick group of the camera update against NP interpolation and Mover's visual smoothing; C2 component versus camera-manager hook versus Gameplay Cameras; C3 dither fade cost with Nanite and Lumen; C4 control-rotation write-back against the sample's look code and the manager's pitch clamps; C5 what the input sim does with `CameraYaw` under non-Z `Up`.

## 12. Risks and open points

| Risk | Mitigation |
|---|---|
| Reimplementing what Epic's Gameplay Cameras or Lyra already provide | Thin stack (a few hundred lines of logic), spike C2 first, bridge later if it wins |
| Control rotation fights the sample's look code or the manager's pitch limits | One writer by policy, `LookInputSource` switch, spike C4 |
| `CameraYaw` under arbitrary gravity is undefined today | C5 first; the camera needs only one deterministic function (§7.2); recorded as a gap in input.md |
| Held direction feels wrong (never releases, or releases too early) | Tunable release (`ReleaseAngleDeg`, `MaxHoldSeconds`), per-section `StickHold`; CAM.4 |
| Fade needs material support and designer marking; unmarked walls pull the camera in | Plugin material function, validator warns about large blockers in a section's probe path, fallback pull-in |
| Converged aim differs from the simulated aim origin by socket motion | Aim origin is the presented socket; error is cosmetic and bounded by quantisation; off switch `bConvergeAim` |

**Milestones** (*Proposal*): **C1** Core, Follow, manual orbit, rule set, settings, adapters, fade, KO hold (replaces the project camera: *tell the user before editing a shared Blueprint*, CLAUDE.md). **C2** Shoulder, aim, crosshair, converged aim, `GSTNCameraInput`:
**needed as soon as a camera-aimed kit exists** (M3, first `AimPolicy = Camera` ability). **C3** sections, editor visualiser, depth-marker bridge, stick hold: **needed by M12 Levels** (game-modes §9.3). Expedition (M13) reuses C1 and C2 with its own
rule set; modules may carry sections. Later: split screen, a shared group-frame mode, a Gameplay Cameras bridge.

## Domande per il proprietario

The questions CAM.1–CAM.6 (in Italian) are in [open-questions-m4-m10.md](open-questions-m4-m10.md), section "Telecamera": automatic return to the designer angle, which shoulder, hold or toggle aim, how long the stick reference is held, per-player or shared camera and split-screen, fade or pull-in on occlusion.
