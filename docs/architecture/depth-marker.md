# Depth marker — ground marker, dashed line and "no ground" ring for depth perception (specification)

Status: **draft for owner review, 2026-10-23**. Presentation-only specification: no simulation state, no network traffic, no
gameplay reads. Owner direction that wins over this file: [game-design-direction.md](game-design-direction.md) §2d (gravity can
change arbitrarily), §2f (KO zones, enemies as platforms), §6b (listen server). The owner's request of 2026-10-23 is quoted
below; **everything else here is *Proposal*** unless a line says *Decision* (already decided in another document).

Scope: which surface a character (or an opted-in enemy) points at while airborne, how the marker is computed, filtered,
smoothed, drawn and budgeted, and where the code lives. Out of scope: any gameplay use of the marker (simulation, AI, hit
resolution and the combat plugins never read it), aim assistance, landing prediction as a gameplay rule, HUD.

Sources: [modularity-and-tags.md](modularity-and-tags.md) §3.7–§3.9, §4.1–§4.2, §6.1; [traversal-surfaces.md](traversal-surfaces.md)
§2–§4 (`Surface.*` behaviours, platform shapes); [combat-m8-minions.md](combat-m8-minions.md) §4d.5, §8.4 (puppets,
`MaxVisiblePuppets`); [many-enemies-replication.md](many-enemies-replication.md) §3.3 (host budgets);
[combat-m4-hits.md](combat-m4-hits.md) §3 (KO zones), §7b (surfaces, trampolines).

Notation: *Decision* = decided elsewhere. *Proposal* = this document's suggestion. *Unsure* = I could not verify it against
engine source or the mod in this session. "Estimate" = a number to measure, not a fact.

> **Revision 2026-10-23:** first version. It folds in the owner rule of the same day that almost everything is a pluggable
> gameplay ability: the marker is **presentation**, so it stays a component plus a subsystem, and every per-pawn on/off switch
> is **data granted with the pawn** (pawn data, Game Feature component, kit ability set), never a code branch (§6.4).

Owner request (2026-10-23, verbatim): "Inoltre: per evitare problemi con la percezione di profondità, sarebbe buono adottare un
sistema alla smash bros melee 3d (la nuova mod uscita), dove c'è un decal, o qualcosa che punta alla piattaforma sotto il
personaggio, più una linea tratteggiata. Anche alcuni nemici dovrebbero averla. Il modo in cui calcolare esattamente quale è la
superficie a cui dovrebbe puntare il decal quando si è a mezz'aria però è ambiguo, quindi ci si dovrebbe pensare un po.
Ovviamente, l'effetto estetico finale potrebbe anche essere, ad esempio un cono d'ombra, o qualcosa di più diegetico che il
designer scioglierà alla fine."

## 0. Summary

- A **depth marker** is a local, per-machine cue: a marker on the surface under a character, joined to it by a dashed line, so
  the player can judge height and horizontal position against platforms in a 3D camera. Some enemies have one (opt-in by data).
- **Default policy**: point at the first valid surface **straight along the entity's own local gravity**, measured from the
  feet, with edge tolerance and hysteresis (§2.2–§2.4). A ballistic landing preview is a separate optional layer, off by default.
- **No ground below**: a **hollow ring on a reference plane** (the owner's second screenshot): level-authored reference volume,
  else the **last grounded height**, else nothing (§2.7). Inside a KO zone the ring doubles as a warning.
- **Never simulated, never replicated.** Computed on every machine, host included, from the *presented* transform with
  asynchronous traces; a dedicated server creates nothing (§3, §6.3). Cost is bounded: 12 markers, 8 traces per frame (§3, §4.5).
- **Replaceable look**: a style data asset lists instanced presenters (decal, ring, dashed line, shadow cone, a diegetic
  Blueprint presenter); target selection is an instanced selector with a C++ default and a Blueprint hook (§5, §6.5).
- **Standalone optional plugin `GSTNDepthMarker`**, no dependency on any other GSTN plugin or the project; Mover, `GSTNSurface`
  and `GSTNMinion` are reached through an adapter plugin and two tiny bridges (§6). **No new tag root** (§6.4).

## 1. Goal and references

### 1.1 What the owner described

The reference is the Smash Bros Melee 3D mod. From the two screenshots the owner attached (I have not seen the mod's code):

1. A character above the main stage has a **reddish filled disc** on the floor directly beneath it; another airborne character
   has a **vertical dashed line** down to the stage.
2. A character **off-stage, over the void and below the stage's top level** shows a **magenta hollow ring** drawn roughly at the
   height of the main platform's surface, **above** the character, joined to it by a vertical dashed line.

*Reading*: the mod has two states. With ground below: filled disc on that ground. With no ground: a differently styled marker on
a **reference plane** (the stage's top level), which tells a player recovering from below how far the lip is. How the mod picks
the plane, or which platform when several are stacked, is not visible in the screenshots: *Unsure*, and not copied; §2 decides.

### 1.2 Similar techniques

A blob or drop shadow straight under the character is the common depth cue in 3D platformers, and some games add a landing ring; I
did not verify any specific game, so I rely on the technique, not on claims about titles. What matters here: (a) a marker straight
along gravity answers "where am I over", not "where will I land" (§2.1); (b) a decal clips to the geometry it lands on, a floating
mesh does not (§4.1); (c) it must survive stacked platforms, gaps and pits, which is the owner's ambiguity.

Non-goals: an aiming tool, a ledge, KO or hit detector, anything in the simulation or the replication.

## 2. The core problem: which surface does the marker point at

### 2.1 Candidate policies

| Policy | How | Platforming | Combat (launched victims, DI) | Readability and stability | Cost, gravity change |
|---|---|---|---|---|---|
| **A. Straight along gravity** | First valid surface along `-Up` from the feet | Tells "what am I over"; with air control the player corrects by watching the disc move | Stable under any launch speed; DI changes shift the disc smoothly; the disc stays under a spinning victim | Best: moves continuously with the character, never depends on input | 1 trace; Up read each frame, so a gravity change re-aims at once |
| **B. Predicted ballistic landing** | Integrate velocity under local gravity until a surface (segments, several traces) | Answers "where do I land", but air control, double jump, air dodge and fast fall all change the answer each frame | Launch speeds are high and DI/SDI change the arc, so the point jumps; useless for a victim in hitstun | Flickers with input; needs the simulation's air-control rules to be right | 3 to 6 segment traces; gravity change invalidates the arc |
| **C. Nearest surface in a cone** | Search below within a cone, take the nearest | Snaps to a platform you are near, even if not over it | Mixed: picks a wall or a platform sideways | Non-physical: the disc is not under the feet, depth cue breaks, two candidates tie often | Sweeps; cone follows Up |
| **D. Hybrid (A + edge tolerance, optional B layer)** | A as the marker, a short sticky hold near edges (§2.4), a *separate* faint landing preview | As A, plus no pop at platform edges; B only for those who want it | A for victims; B never shown to a victim in hitstun | Stable because the primary marker is A | A plus an occasional edge sweep |

### 2.2 Recommendation

**Default selector = D implemented as "A with edge tolerance and hysteresis"** (`UGSTN_DepthTargetSelector_Gravity`). Reason: the
marker is a depth cue, so it must be stable and physically readable; prediction is a different question and depends on input.
B ships as a second selector (`..._Landing`, a smaller ghost marker on a separate presenter) and is **off by default**, only for
the local player and only while the entity is controllable (not in hitstun or tumble). C is not shipped as a default; a designer
can still write it as a Blueprint selector (§6.5).

### 2.3 Algorithm (default selector; the pure parts live in `UGSTN_DepthMarkerMath`, §5)

Per marker, per probe tick (rates in §3). `Up` and `Feet` come from the subject adapter (§5), **never world Z**.

1. **Frame.** `Down = -Up`. If the angle between this frame's `Up` and the last one exceeds `UpResetDegrees` (10°), clear the
   selection state and cross-fade the marker over `GravityFadeMs` (120 ms); a slow, continuous change (curved gravity) keeps
   the state. `Feet` is the presented foot point (§3 note on the presented transform), not the mesh bone.
2. **Gate.** Skip the probe when the subject is grounded and not above `HideHeight`; this is the common case for every
   enemy, so it costs nothing.
3. **Probe.** One multi line trace from `Feet` along `Down`, length `ProbeLength` (6000 cm, the same as the fade end height),
   asynchronous, simple collision only, ignoring the owner, its attachments and every other registered subject.
4. **Candidate filter** (`UGSTN_DepthMarkerMath::FilterCandidates`, pure; inputs are `FGSTN_DepthCandidate`s sorted by distance).
   Reject: non-blocking hits; start-penetrating hits (the feet are inside it; take the next result); candidates **above the
   feet** (`Height < -PenetrationTolerance`, 2 cm, so a thin platform just passed upward is not chosen); faces with
   `dot(Normal, Up) <= 0` (undersides); excluded object types (pawns, projectiles, by settings) and non-targets (§2.8). The
   channel is the gate for triggers and volumes (Overlap or Ignore responses never block).
5. **Choose** the nearest remaining candidate by `Height` (distance along `Down`): the **first surface below the feet**. With
   stacked platforms this is always the one directly under, never a lower floor seen through a thin platform.
6. **Hysteresis and edge hold** (§2.4) decide between this result and the previously selected surface.
7. **Compose the target.** Store the hit as *plane data in the hit component's local space* (`LocalPoint`, `LocalNormal`,
   `SurfaceKey`). Every frame the marker point is the intersection of the subject's column (`Feet + Down * t`) with that plane
   **re-expressed with the component's current transform**. So the marker follows moving bases between traces and hides trace
   latency; traces only revalidate the surface.
8. **No candidate** → reference plane (§2.7) or nothing.
9. **Present** (§3, §4): fade by height, state `Ground`, `EdgeHold`, `NoGround`.

### 2.4 Stacked platforms, gaps and flicker

The centre ray alone flickers when the character crosses a platform edge: the disc would jump to the floor far below, then back.
Rules, evaluated by `UGSTN_DepthMarkerMath::ApplyHysteresis(State, Candidate, FootprintHit, Params, Time)`:

| Rule | Situation | Result |
|---|---|---|
| R1 coplanar | New and previous candidate within `CoplanarTolerance` (2 cm) in height and 5° in normal (overlapping meshes, z-fighting) | Keep the previous `SurfaceKey` |
| R2 nearer wins | New candidate is **nearer** than the previous one (the character moved over a higher platform) | Switch at once; that is the surface it would land on first |
| R3 edge hold | New candidate is **farther** or none, and a short **footprint sweep** (sphere of `FootprintRadius`, issued only in this case, one extra async query) still touches the previous surface within `EdgeForgivenessCm` (40 cm) laterally | Keep the previous surface; the marker point is clamped to the sweep's impact point; state `EdgeHold`, alpha scaled by `1 - lateral / EdgeForgivenessCm` |
| R4 switch | Otherwise | Switch with a cross-fade `SwitchBlendMs` (80 ms); no further switch before `MinDwellMs` (100 ms) |
| R5 gap | Leaving the last surface towards nothing | Same hold as R3, then fade into the `NoGround` state (§2.7) |

A respawn, teleport or pool acquire is a `Teleport`: state cleared, no blend (no streak across the level).

### 2.5 Slopes, alignment, clamping

- **Projection axis is always `Down`** (the "shadow axis"), so the marker stays under the feet on a ramp. A decal projected along
  `Down` stretches naturally on a slope; a mesh presenter instead orients to the surface normal **clamped to `MaxTiltDegrees`**
  (50°) from `Up`, and the marker point is lifted `SurfaceOffsetCm` (0.5 cm) along the normal against z-fighting.
- Radius and alpha come from curves over height (§3); radius is multiplied by `1 / max(dot(Normal, Up), 0.5)` on slopes so a
  disc does not shrink to a sliver (this stretch is cosmetic and capped at 2x).
- Steep faces (`dot(Normal, Up) < MinNormalDot`, 0.2) stay **candidates** (you land on a ramp you will slide down); the style may tint them.

### 2.6 Special situations

| Situation | Behaviour | Why |
|---|---|---|
| **Wall slide** (Mover wall modes) | Marker stays, from the feet straight along gravity (the capsule centre is already a radius off the wall); the decal's angle fade clips it where the wall begins | Height above the floor is exactly what a wall slider wants to read |
| **Grab, link, parked** (`UGSTN_LinkedMovementMode`) | The parked entity's marker is **hidden** (`LinkPolicy = Hide`); the holder or carrier keeps its own | Two markers at the same place add nothing; the policy is data, `ShowOnGround` and `Inherit` exist |
| **Carried, piggyback rider** | Same as parked | M7 keeps rider and carrier together |
| **Moving base, lift, enemy platform** | Follows via local-space plane (§2.3 step 7); stickiness by `SurfaceKey`; a base that vanishes (traversal-surfaces.md §6) is a lost surface → R3/R5 | The marker must not lag behind a fast platform |
| **Trampoline, launch surface** | Valid target (`Surface.Behaviour.Launch`); the style may tint it | Landing on it launches, which the player wants to see coming |
| **Gravity change mid-air** | Step 1: reset and cross-fade when `Up` jumps; the reference plane is re-built from the stored grounded **point** with the new `Up` (§2.7) | Never keep a plane or a trace direction that belongs to the old gravity |
| **Hitstop, hitstun, tumble** | Position follows the presented capsule point, not the tumbling mesh; no landing preview; probes keep their rate | A spinning victim must not spin the marker |
| **Capsule resize, dead or shattering entity** | `Feet` is read each frame, so a resize follows; a dead entity reports `bSuppressed` and the marker fades out | The feet are the origin of every measure; corpses carry none |

### 2.7 No ground below: the reference plane

When step 8 finds no candidate, the target is the **reference plane**, drawn with the `NoGround` presenters (hollow ring by
default, §4). Options considered:

| Option | Pros | Cons |
|---|---|---|
| **Last grounded height**: plane through the point where the entity last stood (stored as a world *point*; the plane is `dot(X - P, Up) = 0` with the **current** `Up`) | Zero authoring; adapts to every level and to gravity changes; matches the screenshot when launched off a stage | Stale after a wall slide or a launch from a high platform; an entity spawned airborne has none |
| **Level-authored reference volume** (`AGSTN_DepthReferenceVolume`: the plane is the volume's top face, in the volume's own up) | Explicit, deterministic, designer-owned; covers ravines whatever the history | Authoring effort; forgotten volumes give no ring |
| **KO plane** (bottom of a KO zone) | Already in the level (M4 §3) | It is *below* the entity, the opposite of screenshot 2; says where you die, not where the lip is; couples to combat |

**Recommendation: reference volume, else last grounded, else none.** With no authoring the mod's behaviour is reproduced by
last grounded; a designer overrides it where it is wrong. The KO plane is **not** used as the ring plane; the warning role is
served by `bHazard` on a reference volume placed in the pit (§3, §8).

Rules: the last grounded point updates only after `GroundedMinMs` (150 ms) on a **static** surface (`bUpdateFromMovingBases` is
off by default, so riding an enemy that walks off a cliff does not move the lip); the ring is shown while the entity is within
`ReferenceMaxDistanceCm` (4000) of the plane, then fades; an entity **above** the plane with no ground (over a gap at the same
level) gets the ring below it, which reads as "no ground here", the intended meaning of hollow.

### 2.8 Valid targets

Targets are found by **collision channel**: `TraceChannel` in the settings (default `ECC_Visibility`; *Proposal*: a project trace
channel `DepthMarker`, *Unsure* whether a plugin can add a channel through its own config, so it is a project setting). A target
is any blocking component that is not a pawn, a projectile (`ExcludedObjectTypes`) or an ignored subject. This includes level
geometry, one-way and thin platforms, **enemy platform shapes** and traversal surfaces. Two requirements:

- Platform shapes must respond **Block to the configured channel** (one line in the profile the platform plugin derives from
  `Behaviour`, traversal-surfaces.md §4.1; *Proposal*, to apply only if this spec is accepted). Neither plugin names the other: the
  shared contract is a collision channel in project settings.
- **Behaviour filter**: `TargetBehaviours` is an `FGameplayTagQuery` over the surface's behaviours (default in the `GSTNSurface`
  bridge: any of `Surface.Behaviour.Stand`, `Surface.Behaviour.Launch`; empty = channel only). Surfaces with no behaviour data pass
  (`bUnknownSurfacesPass = true`, the same rule as traversal-surfaces.md §2.2 step 4). A wall-slide-only flank is rejected, which
  rarely matters because a gravity ray grazes it.

## 3. Visibility rules

| Rule | Default | Why |
|---|---|---|
| Shown when | the adapter says `bAirborne` **and** height above the target ≥ `ShowHeight` (60 cm); hidden below `HideHeight` (40 cm) | Hysteresis; nothing to read while standing |
| Fade by height | `AlphaOverHeight` and `RadiusOverHeight` curves in the style (alpha 1 → 0.25, radius 1 → 0.6 over 0..`FadeEndHeight`) | Mario-style shrinking cue; far surfaces stay faint, not absent |
| `NoGround` shown | within `ReferenceMaxDistanceCm` of the plane, regardless of `ShowHeight` | The ring is the main cue off-stage |
| Whose | **local player(s)** always; allies per `Audience` (`LocalOnly`, `LocalAndAllies`, `All`; default `LocalAndAllies`, co-op); enemies **only if opted in by data** (§6.4) | The owner asked for it on some enemies only |
| Audience source | `IGenericTeamAgentInterface` when the pawn has it; otherwise the adapter's `GetTeamId` | No team plugin dependency |
| Distance cull | local player none; others beyond `MaxViewDistanceCm` (4000) get no probe and no marker | Cost control |
| Max count | `MaxMarkers` 12 (4 players + 8 others); ranked: local player, allies, then enemies by screen proximity, distance, recently hit | Bounded draw cost |
| Probe rate | rank 0–3 every frame; rank 4–7 30 Hz; the rest 10 Hz; total ≤ `MaxTracesPerFrame` (8), oldest first | Traces dominate the cost |
| Hidden by | `LinkPolicy`, `bSuppressed`, `HideWhen` tag query (§6.4), the player's accessibility setting | Per-pawn and per-player switches |
| Dedicated server | subsystem not created (`NM_DedicatedServer`) | Nothing to draw |
| Listen host | creates and renders it like a client (*Decision* §6b: the host renders); budget shared with the host's frame (§7) | The host plays |

**Presented transform.** The marker reads the transform that is drawn: on the owner's machine the actor, on remote clients the
interpolated proxy (the NP interpolated state, ~100 ms behind the host, many-enemies §1.3), for minion puppets the puppet's
interpolated sample. It never reads simulation state, so it follows rollbacks and corrections exactly as the mesh does. Reading
component state is allowed here: the "no Mover queries" rule of CLAUDE.md applies to simulation code only.

**Tick.** The subsystem ticks after movement and animation (a tick function in `TG_PostUpdateWork`, as `UGSTN_CombatSimComponent`
does). *Unsure*: that NP's interpolated transform of a simulated proxy is applied before that group; spike D1 (§7) checks it.
**Controls:** `gstn.DepthMarker.Enable`, `.MaxMarkers`, `.TracesPerFrame`, `.Debug`, `.Stats`; a per-player on/off and intensity
option (accessibility; not per-pawn data).

## 4. Rendering

### 4.1 Layers and techniques

| Layer | Technique | Why |
|---|---|---|
| Ground disc | **Decal** (`UDecalComponent`), box projected along `Down`, size from radius | Clips to geometry and edges, follows slopes; no floating disc hanging off a platform edge |
| No-ground ring | **Translucent unlit mesh** (flat ring on the plane), oriented to `Up`, no depth fade | A decal needs a receiver; there is none in the void |
| Dashed line | **Camera-facing quad** from the feet to the marker point | One mesh, shader-driven dashes (§4.2) |
| Shadow cone | Cone mesh, apex at the feet, base = disc radius on the target | The owner's "cono d'ombra"; combines disc and line |
| Diegetic | A designer **presenter** (Blueprint) driving a Niagara system or actor | The owner's "something more diegetic", decided at the end |

A style enables layers per state (a style may have only a cone). If decals are off (platform or quality), the mesh presenter draws a quad.

### 4.2 Dashed line

A unit quad scaled to the line length. In the vertex shader the quad is expanded perpendicular to both the line direction and the
camera vector (axis-aligned billboard), with width in cm or constant screen pixels (`bConstantScreenWidth`). The pixel shader
masks dashes from the **world distance along the line**: `frac((s + Time * MarchSpeed) / DashPeriodCm) < DutyCycle`, smoothstep
edges, so the dashes keep size whatever the length and, with a small `MarchSpeed`, drift towards the target (a motion cue for
"down"). The same material serves line and cone outline. Line alpha = marker alpha, fading out in the last `LineFadeNearCm`.

### 4.3 Drawing over occluders

The disc is **depth tested** (if the ground is hidden behind a wall so is its marker). The dashed line and the ring use a
translucent material with **depth test disabled** and a high `TranslucencySortPriority`; the shader compares `PixelDepth` with
`SceneDepth` and lowers alpha to `OccludedAlpha` (0.3) where occluded. *Unsure*: cost on the target platform; the fallback is
plain depth-tested materials. A character hidden behind a pillar still shows a faint line to the floor.

### 4.4 Colour and identity

Tint per marker: the style's `TeamTints` by team id, a per-component `TintOverride`, or `GetTint` from the adapter; default per
player index. Colour alone is not enough: the style may add a **glyph or pattern id** (custom primitive data) per player, for
colour-blind players. Tint, alpha, radius and dash length travel as **custom primitive data**, never one material instance per marker.

### 4.5 Cost and batching (stages)

- **S1 (first delivery):** per-marker pooled components (decal, ring mesh, line mesh), `MaxMarkers` 12. Estimate: ≤ 0.25 ms mean
  game thread for 12 markers including trace issue and presenters, ≤ 0.4 ms p99; ≤ 0.3 ms GPU for 12 decals at 1080p. To measure (§7).
- **S2 (if S1 misses on the host, or many enemies opt in):** one `UInstancedStaticMeshComponent` per style for ring, line and
  cone with per-instance custom data (one draw each); decals stay components (not instanceable) with a lower cap.
- **S3 (stretch):** a Niagara system fed by array data interfaces (one draw for all markers) or a **depth-projected box mesh**
  (a mesh material that reconstructs world position from scene depth, which gives decal-like clipping and can be instanced).
  *Unsure*: Niagara's decal renderer availability and cost in 5.8; the depth-projected box needs a prototype.

## 5. Types

Naming: `UGSTN_`, `FGSTN_`, `EGSTN_`, export `GSTNDEPTHMARKER_API`; stateless maths is a `*Math` function library (the project uses
`*Utils`; the name follows the content: pure geometry).

| Type | Module | Kind | Role |
|---|---|---|---|
| `FGSTN_DepthSubjectState` | Core | USTRUCT | What the adapter reports each frame (table below) |
| `FGSTN_DepthCandidate` | Core | USTRUCT | One trace hit reduced to numbers: `Height`, `Point`, `Normal`, `SurfaceKey` (opaque `uint64`), `BehaviourTags`, flags `bBlocking`, `bStartPenetrating`, `bMovingBase`, `bExcluded` |
| `FGSTN_DepthTarget` | Core | USTRUCT | Result: `Kind {None, Surface, ReferencePlane}`, `Point`, `Normal`, `Height`, `SurfaceKey`, `LocalPoint/LocalNormal`, `EdgeAlpha`, `bHazard` |
| `FGSTN_DepthSelectionState` | Core | USTRUCT | Per marker memory: previous `SurfaceKey`, height, last switch time, last grounded point, last `Up` |
| `FGSTN_DepthSelectorParams` | Core | USTRUCT, BlueprintType | Tuning of §2 (table below) |
| `UGSTN_DepthMarkerMath` | Core | BlueprintFunctionLibrary | `FilterCandidates`, `ChooseFirstBelow`, `ApplyHysteresis`, `ProjectOntoPlane`, `ClampTilt`, `ReferencePlane`, `ComputeFade`, `PredictBallistic` (pure, `BlueprintPure`) |
| `UGSTN_DepthMarkerSettings` | Runtime | UDeveloperSettings | Project defaults and caps (below) |
| `UGSTN_DepthMarkerStyle` | Runtime | PrimaryDataAsset | Look: `TArray<Instanced UGSTN_DepthMarkerPresenter> Presenters` with `ActiveStates`, curves, tints, `OccludedAlpha`, `LinkPolicy` |
| `UGSTN_DepthMarkerPresenter` | Runtime | Abstract, Blueprintable, EditInlineNew | `BlueprintNativeEvent OnMarkerUpdated(const FGSTN_DepthMarkerView&)`, `OnStateChanged`; built-ins `_Decal`, `_Ring`, `_Line`, `_Cone` |
| `UGSTN_DepthTargetSelector` | Runtime | Abstract, Blueprintable, EditInlineNew | `BlueprintNativeEvent BuildProbes(Subject) → probes`, `ChooseTarget(Subject, Hits, State) → Target`; built-ins `_Gravity` (default), `_Landing` |
| `UGSTN_DepthSurfaceResolver` | Runtime | Abstract, Blueprintable, EditInlineNew | `ResolveSurface(const FHitResult&) → {Behaviours, bTarget}`; default: unknown, passes |
| `IGSTN_DepthSubject` | Runtime | UINTERFACE | An actor that reports its own `FGSTN_DepthSubjectState` (BlueprintNativeEvent) |
| `IGSTN_DepthSubjectProvider` | Runtime | UINTERFACE | A batch source of subjects that are not actors with components (minion puppets); registered with the subsystem |
| `IGSTN_DepthTagSource` | Runtime | UINTERFACE | Optional: the pawn's tags for `HideWhen` (§6.4) |
| `UGSTN_DepthSubjectAdapter` | Runtime | Abstract UObject | Reads a pawn's movement component into a state; factory registered in `FGSTN_DepthAdapterRegistry` |
| `UGSTN_DepthMarkerComponent` | Runtime | ActorComponent, Blueprintable | The opt-in: `Style`, `Selector`, `Audience`, `TintOverride`, `HideWhen`; registers with the subsystem; **never replicated** |
| `UGSTN_DepthMarkerSubsystem` | Runtime | UWorldSubsystem + tick | Registry, ranking, probe scheduler, async trace, cache, pool, presenter drive |
| `AGSTN_DepthReferenceVolume` | Runtime | AVolume | Reference plane (§2.7): `Plane` mode (`TopFace`, `FixedOffsetAlongUp`), `Priority`, `bHazard` |

`FGSTN_DepthSubjectState` (all in world space, presented values):

| Field | Meaning |
|---|---|
| `Feet`, `Up` | Foot point and unit up (opposite of the entity's local gravity this frame) |
| `bAirborne`, `bGrounded` | From the movement component; `bGrounded` also feeds the last grounded point |
| `FootRadius`, `Velocity` | Footprint sweep radius and decal scale; velocity for the optional landing selector |
| `BaseComponent` (weak) | What the entity stands on, for `bMovingBase` and the grounded point |
| `bOnWall`, `bParked`, `bSuppressed` | Wall mode, linked/parked, dead or hidden; derived from **mode names given as data** on the adapter (`WallModeNames`, `ParkedModeNames`), so the adapter knows no project mode |
| `TeamId`, `Tint` | Optional, for audience and colour |

`FGSTN_DepthSelectorParams` holds every named tolerance of §2 (`ProbeLength`, `PenetrationTolerance`, `CoplanarTolerance`,
`EdgeForgivenessCm`, `FootprintRadiusScale`, `MinDwellMs`, `SwitchBlendMs`, `MinNormalDot`, `MaxTiltDegrees`, `SurfaceOffsetCm`,
`UpResetDegrees`, `GravityFadeMs`, `GroundedMinMs`, `ReferenceMaxDistanceCm`, `bUpdateFromMovingBases`), each `EditAnywhere,
BlueprintReadOnly`, `Category = "DepthMarker|Selection"`, with `ClampMin` and `ForceUnits`.

`UGSTN_DepthMarkerSettings`: `bEnabled`, `TraceChannel`, `ExcludedObjectTypes`, `TargetBehaviours` (query), `bUnknownSurfacesPass`,
`DefaultStyle` and `DefaultSelector` (soft class), `DefaultSurfaceResolver`, `MaxMarkers`, `MaxTracesPerFrame`, `MaxViewDistanceCm`,
rank rates, `Audience`, `ShowHeight`/`HideHeight`/`FadeEndHeight`, `DefaultParams`. Defaults live in the plugin's
`Config/DefaultGSTNDepthMarker.ini` and point to the plugin's own `Content/` (§3.7 rule 1, §3.9).

**Default adapters** (a factory registry picks one by priority, so `GSTNDepthMarkerRuntime` knows no Mover type):
- **Mover** (`GSTNDepthMarkerMover`): reads `UMoverComponent::GetUpDirection()`, `GetVelocity()`, `GetMovementBase()` and, for a
  `UCharacterMoverComponent`, `IsAirborne()` / `IsOnGround()` (verified in the repo's Mover override headers); feet from the capsule
  half height along `Up`. Mode names for wall and parked come from data. *Unsure*: where Mover's visual smoothing applies, so the
  adapter lets the pawn name the component whose transform is the presented one (default root).
- **CharacterMovement** (in `GSTNDepthMarkerRuntime`): `UCharacterMovementComponent` (`IsFalling()`, velocity, floor, base);
  `Up` from `GetGravityDirection()`, *Unsure* of the exact 5.8 name, else the physics volume's gravity.
- **Actor fallback**: actor location, `Up` from world gravity, `bAirborne` from vertical speed; for props and tests.

## 6. Plugin and module boundaries

### 6.1 Plugins and modules

| Plugin / module | Type | Depends on | Contents |
|---|---|---|---|
| **`GSTNDepthMarker`** / `GSTNDepthMarkerCore` | Runtime | `Core`, `CoreUObject`, `GameplayTags` | The pure types and `UGSTN_DepthMarkerMath` of §5; no world, no Engine; Automation-tested alone |
| `GSTNDepthMarker` / `GSTNDepthMarkerRuntime` | Runtime | Core module, `Engine`, `DeveloperSettings`, `GameplayTags` | Subsystem, component, presenters, selectors, CharacterMovement and actor adapters, settings, reference volume, console variables; created unless `NM_DedicatedServer` |
| `GSTNDepthMarker` / `GSTNDepthMarkerEditor` | Editor | Runtime, `UnrealEd` | Style and settings validators, preview of a style in the viewport, volume visualiser |
| **`GSTNDepthMarkerMover`** (adapter plugin, one module) | Runtime | `GSTNDepthMarker` (Runtime), `Mover` | Registers the Mover adapter; exists so a project without Mover can use the main plugin (*Unsure*: whether an optional plugin reference in the `.uplugin` would be enough, so a separate plugin is the safe choice) |
| **`GSTNDepthMarkerSurface`** (bridge, later) | Runtime | `GSTNDepthMarker`, `GSTNSurfaceCore` | A `UGSTN_DepthSurfaceResolver` that calls `UGSTN_SurfaceUtils::Resolve`; sets the default `TargetBehaviours` query; no tags |
| **`GSTNDepthMarkerMinion`** (bridge, later) | Runtime | `GSTNDepthMarker`, `GSTNMinionRuntime` | `FGSTN_DepthMarkerMinionExtension { Style, bShow }` on `UGSTN_MinionDefinition`; an `IGSTN_DepthSubjectProvider` that publishes the **visible, opted-in puppets** from the presentation subsystem each frame (position, `Launched` or flying state, gravity-up from the store; *Unsure* what the store exposes for up, M8 owns it) |

Layer: an optional presentation feature (L4 in modularity §3.1) that depends on **L1 only**. The two bridges and the adapter follow the
bridge rule (§3.2, §3.7 rule 3): each depends on exactly the main plugin and one other, nothing depends on a bridge, and `GSTNMinion`,
`GSTNSurface` and Mover never know the marker. `EnabledByDefault: false` for all four plugins.

### 6.2 Must never depend on

`GASP_Giustino2` and any `/Game/...` content; `GSTNCombat*`, `GSTNInput*`, `GSTNMinion*`, `GSTNPlatform*`, `GSTNParts*`,
`GSTNLink*`, `GSTNDefence*` (outside the named bridges); `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction`;
`Mover` outside the adapter plugin; any Editor module from a runtime module. `check_modules.py` rules 5, 10, 11 and 12 apply
(`ConstructorHelpers` and literal asset paths are forbidden; defaults are soft references). Canary build for `GSTNDepthMarker` alone
and for the project with all four removed (nothing else changes, §3.7 rule 7).

### 6.3 Networking and simulation

No replicated property, RPC, sync or aux state, or fragment; the component sets `SetIsReplicatedByDefault(false)` and the subsystem is
never spawned on a dedicated server. Computation is local on the host and every client from presented transforms. No `*NP` module,
ability or hit step includes a marker header, so nothing can read it: rollback-safe by construction (CLAUDE.md rule 1 does not apply).

### 6.4 Tags, and per-pawn switches as data (owner rule)

**The plugin declares no gameplay tag and no root**, so §4.1 needs no owner entry. It reads: `Surface.Behaviour.*` through the
`TargetBehaviours` query (owned by `GSTNSurface`; resolved by the bridge, or `RequestGameplayTag(..., false)` when present;
absent = empty query = channel only, §3.7 rule 4). Tuning is data fields, not tags (§4.1 rule 6).

The marker is not an ability (presentation), but per the owner rule **no switch is a code branch**:

1. **Static opt-in = a component granted by data.** A pawn has a marker if it has `UGSTN_DepthMarkerComponent`: added by the pawn data
   or the ability set of the kit that wants it (§3.4 pawn data), by a Game Feature `AddComponents` action (modular gameplay), or, for
   minions, by the definition's extension (data on the enemy type). Enemy types without it have none. No `if (IsEnemy)` anywhere.
2. **Style and selector per pawn** are soft references on that component, so an enemy type or a kit picks its look in its data.
3. **Dynamic on/off = a tag query on the component** (`HideWhen`, `ShowOnlyWhen`, empty by default), evaluated against tags from
   `IGSTN_DepthTagSource`. The project's pawn (or one small project component) implements it from its ASC, and a kit's ability set
   grants the tag with a gameplay effect, so a flying kit hides the marker by data. The tags are the content owner's (an ini tag
   under its own root, §4.2b); the plugin names none. Presentation only: the query is read locally, never by the simulation.
   *Unsure*: whether such tags are visible on remote simulated proxies in the fork (combat.md: abilities do not run there, tags are
   projected from sync state). Static opt-in (item 1) works everywhere; the dynamic switch is guaranteed only where the tag is visible.
4. **Per-player** options (accessibility, intensity) are user settings, not pawn data.

### 6.5 Designer extension (§3.8) and replaceable content (§3.9)

- **Look**: swap the `UGSTN_DepthMarkerStyle` asset (settings default < component override < game mode or experience data). Presenters
  are `Blueprintable` instanced objects; a Blueprint presenter can drive any actor or Niagara system (the diegetic option).
- **Target selection**: `UGSTN_DepthTargetSelector` has `BlueprintNativeEvent`s taking a const input view and returning a result
  struct; the C++ `_Gravity` selector is the default `_Implementation`, and a Blueprint child can call the parent or replace it (cone,
  landing). Presentation, so Blueprint is free (no determinism contract), but per-frame Blueprint cost counts against §4.5; hot work
  stays C++ in `UGSTN_DepthMarkerMath` (`BlueprintPure`, callable from a hook).
- The plugin ships an example Blueprint selector and presenter, README sections "Extending in Blueprint" and "Replacing content", and
  its own minimal map (§7), per modularity §7.1.

## 7. Testing

**Automation, pure (`GSTNDepthMarker.Core.*`, no world):** `FirstSurfaceBelowIgnoresAbove`, `StackedPlatformsPickNearest`,
`StartPenetratingSkipped`, `ThinPlatformJustPassedIsAboveFeet`, `UndersideRejected`, `CoplanarOverlapKeepsPrevious`,
`NearerSurfaceSwitchesAtOnce`, `EdgeHoldWithinForgiveness`, `SwitchBeyondForgivenessWithBlend`, `MinDwellPreventsPingPong`,
`PitUsesReferenceVolume`, `PitUsesLastGroundedPlane`, `NoReferenceMeansNone`, `RingAboveEntityWhenBelowPlane`, `GravityFlipUsesNewUp`,
`SidewaysGravityPlane`, `UpResetOnlyOnJump`, `SlopeTiltClamped`, `MovingBaseLocalPlaneFollows`, `FadeCurvesMonotonic`,
`BallisticLandingUnderGravity`, `BallisticAbortsOnGravityChange`. Tests use several `Up` vectors, diagonal ones included, so a hidden
world-Z assumption fails.

**Automation, runtime (`GSTNDepthMarker.Runtime.*`, editor world):** `SchedulerRespectsTraceCap`, `RankingKeepsLocalPlayer`,
`CapDropsLowestRank`, `GroundedSkipsProbe`, `AdapterRegistryPriority`, `CharacterMovementAdapterReadsGravity`,
`DedicatedServerCreatesNothing`, `ComponentNotReplicated`, `TriggerPawnProjectileNeverTargets`, `StyleValidates`.

**Test map `L_DepthMarker_Min`** (plugin content, one plugin-owned pawn Blueprint, §3.5): flat floor and jump; three stacked
platforms; a one-way thin platform; a short and a long gap; ramp and stairs; a pit with and without a reference volume; a gravity
flip and a sideways gravity volume; a lift and a sliding platform; an enemy platform proxy (once `GSTNPlatform` exists); a wall; a
trampoline; a trigger volume, a pawn and a projectile under a platform (must not be targets); a 12-flyer stress area.

**Harness (listen host + 2 clients, then dedicated + 2 clients):** `scenario_depthmarker_remote` (a remote pawn's marker within 1 cm
of its mesh at running speed, and **zero** bytes attributable to the plugin), `scenario_depthmarker_gravity` (flip during a jump:
one cross-fade, no streak). Spikes: D1 tick group against NP interpolation (§3); D2 decal versus mesh ring on Nanite and Lumen
scenes, 12 markers at 1080p; D3 async trace latency and cost at 8 traces per frame while the minion batch runs on the host.

**Performance check with many enemies:** the M8 bench (150 population, about 60 awake, `MaxVisiblePuppets` 60) with 20 minion types
opted in through the bridge: the subsystem must hold ≤ 12 markers and ≤ 8 traces per frame whatever the opted-in count, mean
≤ 0.25 ms and p99 ≤ 0.4 ms on the **host** (estimates, measured together with the host's frame as in many-enemies §3.3), stat group
`STATGROUP_GSTNDepthMarker`, Unreal Insights tracks. Fail ⇒ lower `MaxMarkers`, then the rank rates, then S2 (§4.5).

## 8. Risks and open points

| Risk | Mitigation |
|---|---|
| The straight-down marker is not what the owner wants for "where will I land" | `_Landing` selector as a separate layer; question DM.1 |
| Last grounded plane is wrong after a wall slide, a high launch or an airborne spawn | Reference volumes override it; moving bases do not update it; no plane means no ring |
| Enemy platform shapes ignore the marker channel, so players see the floor below an enemy platform | One profile line in the platform plugin (§2.8); test map entry; validator warning in the platform plugin |
| Characters or projectiles become targets (a Block default of a new channel) | Ignore list of subjects, `ExcludedObjectTypes`, tests `TriggerPawnProjectileNeverTargets`; *Unsure* of the default response of a new channel |
| Marker drifts from the mesh on remote clients | Read the presented transform; spike D1 and `scenario_depthmarker_remote` |
| Async trace latency shows on fast platforms | Local-space plane reprojection every frame (§2.3 step 7) |
| Decals not available or costly (Nanite, Lumen, forward renderer) | Mesh fallback presenter; spike D2; style can drop the decal |
| Occlusion material too costly | Plain depth-tested variant; stage S2 batching |
| Many opted-in enemies on a listen host | Caps are global; the provider publishes only visible, airborne, opted-in puppets; cost measured with the host's frame |
| Mover's visual smoothing component unknown | The pawn names the presented component in the adapter data; spike D1 |
| Open: hazard styling in KO zones; hide tag on remote proxies | `bHazard` on a reference volume (DM.9); static opt-in works everywhere, dynamic switch best-effort (§6.4) |

## 9. Questions for the owner

The questions DM.1–DM.9 (in Italian, for the owner) are in [open-questions-m4-m10.md](open-questions-m4-m10.md), section "Indicatore di profondità". Each one states the hypothesis this spec is written on, so the spec stands until an answer changes it. The main one is DM.1: straight down along local gravity (the default selector, §2.2) or the predicted landing point (an optional second selector).
