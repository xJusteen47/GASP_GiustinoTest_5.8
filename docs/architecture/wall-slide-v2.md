# Wall slide v2: review of the current mode and design of a parallel, leaner mode

Status: draft for owner review, 2026-10-23. Docs only, no code. Source reviewed: branch `feature/gas-npp-5.8` as of
2026-10-23 00:15 (`Source/GASP_Giustino2/{Public,Private}/WallMovement/`, `GAS/GSTN_GA_WallSlide.*`,
`GAS/GSTN_AbilityWallEntryTransition.*`). Related: [wall-movement.md](wall-movement.md) (the current pipeline),
[traversal-surfaces.md](traversal-surfaces.md) §4.3/§5.2 (T2 moving bases, T5 `FGSTN_WallSurfaceFilter`),
[plans/README.md](plans/README.md) (approved `IsMovementLocked` core edit, H7), [game-design-direction.md](game-design-direction.md)
§2d (gravity is arbitrary: "up" = -gravity, never world Z).

Owner request (2026-10-22, verbatim): "ti consiglio di vedere anche una revisione, o un'altra wallslide movement mode, che
permette di diminuire la velocità quando si passa per spigoli, ma non nel caso di rilievi/cambi di scoscesità piccoli. In
quest'ultimo caso, una logica stile stepup dovrebbe avere la precedenza. Ma non rimuovere la modalità corrente. Farò dei test
e deciderò quale pluggare, al massimo rinominiamo la corrente e la chiamiamo _legacy. La modalità di wallslide corrente mi
sembra parecchio bloated, e in alcuni casi poco elegante, con troppi calcoli. Dovremmo ripassare su questo punto."

Line references are `file:line` in the files above; `Base` = `GSTN_WallMovementModeBase.cpp`, `Base.h` = its header,
`Query` = `MoveLibrary/GSTN_WallQueryUtils.cpp`, `MoveUtils` = `MoveLibrary/GSTN_WallMovementUtils.cpp`,
`Slide` = `Modes/GSTN_WallSlideMovementMode.cpp`, `Entry` = `Transitions/GSTN_WallEntryTransition.cpp`,
`Types.h` = `GSTN_WallMovementTypes.h`.

## 1. Review of the current wall slide

`UGSTN_WallSlideMovementMode` itself is 48 lines (`Slide:10-48`): defaults, a vertical model that routes through the base's
CMC-style axis and adds gravity. Everything the owner perceives as bloat lives in what it inherits: `UGSTN_WallMovementModeBase`
(609 lines, 25 tunables), `UGSTN_WallQueryUtils` (390) and `UGSTN_WallMovementUtils` (503). The core was written for free
climbing on any surface (ceilings, overhangs, moving walls, corners both ways) and the slide pays for all of it on every tick.

### 1.1 Queries per tick (common path: sliding down a flat vertical wall, one substep)

| Step | Where | Queries |
|---|---|---|
| Move along the wall | `Base:371` `TrySafeMoveUpdatedComponent` | 1 capsule sweep |
| Along-wall probes (center/top/bottom) | `Query:77-91` | 3 sphere sweeps |
| Edge validation of each accepted probe hit | `Query:102` → `IsHitOnValidWall` `Query:206` | 0-3 line traces (only when the sphere normal and the face normal differ by > 2°, i.e. near edges) |
| Lookahead probe (runs whenever velocity ≠ 0, i.e. always while sliding) | `Query:157` | 1 sphere sweep (+1 line trace +1 overlap test when it hits: `Query:161-162`) |
| Orientation | `MoveUtils:337` `OverlapTest` | 0 at steady state (`Base:440` skips when converged), 1-3 while converging |
| Adhesion sweep | `MoveUtils:373` | 1 capsule sweep |
| Adhesion correction | `MoveUtils:399` (`bSweep = true`) | 0-1 capsule sweep (whenever the gap error > 0.01 cm, i.e. on any non-flat wall every tick) |

**Baseline: 6 shape sweeps per tick, 7-11 queries on a slightly uneven wall or while the capsule is still turning.** Each
substep repeats the whole block (`Base:309-359`, up to `MaxSubsteps = 4`), and the non-common branches add: blocking hit →
`IsConcaveCornerHit` (`Query:242-265`: 1 line trace + 1 overlap) + `TryMoveToSlideAlongSurface` (`Base:404`, 1-2 sweeps);
wall lost → `FindWallAroundConvexEdge` (`Query:347-357`: 2 sphere sweeps + 1 line trace). Worst case per substep ≈ 13 sweeps
+ 6 line traces + 5 overlaps, ≈ 90 queries per tick at 4 substeps. For comparison Mover's `USimpleWalkingMode` costs ~3 per tick.

### 1.2 Redundant or duplicated queries and math

1. **The lookahead and the move re-validate the same corner.** A blocking hit in `MoveSubstep` runs `IsHitOnValidWall` +
   `CanFitOnWall` (`Query:256-264`) and hands the corner to `FindWall` as a sample (`Base:395-396`, `Query:298-301`); in the
   same substep the lookahead sweep hits that same wall and runs `IsHitOnValidWall` + `CanFitOnWall` again (`Query:161-162`).
   Two overlap tests and two line traces for one inside corner, every substep until the plane has turned.
2. **Adhesion and contact probing measure the same thing twice.** The adhesion capsule sweep (`MoveUtils:373`) already returns
   the wall hit (normal, impact point, distance, component) at the capsule itself; the three sphere probes (`Query:77-91`)
   measure the same wall again, from the same center, in the same direction. The sphere's "rounded" normal is the stated
   reason (`Query:63-66`) but a capsule sweep rounds edges the same way.
3. **The entry transition sweeps twice per grab.** `Trigger` re-runs `FindEntryWall` (`Entry:32-45`) right after `Evaluate`
   did (`Entry:26-30`): up to 2 capsule sweeps + 2 line traces (`Entry:102`, `Entry:125`) repeated, on top of Mover calling
   `Evaluate` more than once per falling tick. Not a slide-tick cost, but the same pattern.
4. **Capsule size and collision params are rebuilt per function, not per tick.** `GetCapsuleSize` is called 4-6 times per
   substep (`Base:299`, `Query:283`, `Query:263`, `Query:322` wrap, `MoveUtils:322`), `FQueryContext` /
   `InitCollisionParams` 3-5 times (`Query:17`, `Query:256`, `Query:276`, `Query:317`, `MoveUtils:364`, plus `OverlapTest`).
   Cheap individually; together they show that the per-substep context has no home.
5. **The skid decision is computed twice** (`GSTN_SkiddingWallMovementMode.cpp:12-31` rebuilds plane + intent + skid that
   `GenerateMove` already did). Documented as a Mover constraint (`GenerateMove` is const and cannot publish) and the slide
   mode does not inherit it, so it is fair; it is still a cost the design should avoid by construction.

### 1.3 Unclear responsibilities and coupling

6. **Two sources of truth for the query tunables.** `FGSTN_WallCheckSettings` (`Types.h:358-410`) duplicates 12 mode
   properties (`Base.h:115-170`, `Base.h:193-195`) and `GetWallCheckSettings` copies them every tick (`Base:566-582`). The struct
   is `BlueprintType` and carries a `bDrawDebug` (`Types.h:408-409`) that is not a query parameter.
7. **One knob, several meanings.** `WallProbeDistance` is the probe reach (`Query:73`) *and* the adhesion reach
   (`MoveUtils:370`), documented under two categories. `NormalDeadzoneAngle` (a filter parameter) is passed as the minimum
   corner angle of `IsConcaveCornerHit` (`Base:382-383`). `WallGap` is in the probe reach, the fit test and the adhesion.
8. **`FindWall` is three things with magic weights.** Spatial filter (3 probes, weights 1/0.5/0.5, `Query:77-82`), temporal
   anticipation (lookahead, `1.5 * Closeness²`, `Query:165`) and physical contact (corner hit, weight 2, `Query:300`) are
   summed into one normal. The `Agreement²` factor (`Query:108-113`) exists to stop a self-sustained oscillation that the
   design itself creates (probe direction = filtered normal, which depends on the probes). Likewise the "frozen at rest"
   rule in `AttachToWall` (`Base:419-432`) exists because normal → orientation → probe origins (`Up * VerticalOffset`,
   `Query:74`) → normal is a feedback loop. Three mechanisms to tame one loop.
9. **The movement plane carries the capsule orientation** (`Types.h:439-440`, `MoveUtils:49`) although the header's rule is
   that orientation never feeds back into the plane (`Base.h:28-34`). It is needed only for the ceiling fallback and the
   surface-relative input mapping, neither of which the slide uses.
10. **The lookahead/corner sample never names a base component.** Only along-wall samples set `OutResult.Component`
    (`Query:117-122`); turning onto a moving wall at a corner rides the old base for a tick. Minor, relevant to T2.

### 1.4 Behaviour at edges today (the owner's actual complaint)

11. **No corner slows the character.** `TransportVelocity` is length-preserving by design (`MoveUtils:193-201`, "speed is
    preserved around corners", `Base.h:48`); for climbable hits `MoveSubstep` never reduces the velocity (`Base:393-397`), only
    for non-climbable ones (`Base:398-402`). So a 90° inside corner and a 5° slope change are treated the same: the plane turns,
    the speed stays.
12. **A small side bump stops the slide dead.** Sliding sideways into a 5 cm protrusion: the move sweep blocks on the bump's
    side face, `IsConcaveCornerHit` accepts the angle but `CanFitOnWall` (`Query:216-240`) fails ("faces narrower than the
    capsule are rejected"), so the hit is treated as a non-climbable obstacle, the velocity is projected onto the bump face
    (`Base:398-402`) and the capsule stays pressed against it with zero sideways speed. The adhesion then cannot lift it over.
13. **A small ledge below ends the slide.** Sliding down onto a protrusion whose top face is flatter than `MinSurfaceAngle`:
    `IsFloorHit` (`Query:267-271`) is true → `LeaveTo(FloorModeName)` (`Base:386-391`) → Walking, from which the character
    falls off and must re-grab subject to the height cap. There is no step-over, which is exactly the "rilievi" case.
14. **Small slope changes are already handled well**: deadzone + rate limit (`MoveUtils:207-218`) and probe rounding. This is
    the part to keep.

### 1.5 Determinism and rollback

15. **Other pawns are walls.** Every probe uses the capsule's own channel (`Query:30`, `Query:36`) and ignores only the owner;
    a side of another player's capsule passes `IsValidWallNormal` (horizontal normal). On a client the other pawns are at
    interpolated positions, on the server at simulated ones, so a probe that grazes a pawn produces different normals on each
    side; the entry transition can grab a pawn as a wall the same way (`Entry:102`). Real divergence risk in multiplayer.
16. **Game-thread reads in the simulation**, both documented: the crouch-intent detach fallback (`GSTN_ControlledWallMovementMode.cpp:146-151`,
    `bUseCrouchAsDetachButton = true` by default) and owner tags for `MemoryResetTags` (`Entry:70-84`). The slide mode has
    `bDetachByButton = false`, so the first is inert today; it is still a trap one checkbox away.
17. **`WallContactPoint` is not reconciled** (`GSTN_WallMovementTypes.cpp:155-159`) but it is the rotation pivot
    (`Base:442`, `MoveUtils:335`), and the pivot changes the capsule location. It is a weighted average of probe points, so it is
    deterministic for equal inputs; after a position correction it is simply re-sampled. Low risk, worth knowing.
18. **Substep count depends on speed and on the default `MaxSubstepDistance` derived from the capsule radius** (`Base:302-304`);
    the normal filter's rate limit is applied per substep (`Base:426`), so the filter's effective time constant changes with
    the number of substeps. Deterministic, but the tuning value does not mean what it says at all speeds.
19. The non-rollback `SimBlackboard` writes (`Base:230-235`) are idempotent invalidations, same as Mover's own modes: fine.
    The rollback blackboard entries (`BaseInfo`, `EntryInfo`), the sync state and the reset effect are correct.

### 1.6 Dead or over-general code (seen from the slide)

20. **Ceiling support the slide cannot use.** With `MaxSurfaceAngle = 120` (`Slide:23`) the ceiling branches are unreachable:
    the plane fallback (`MoveUtils:57-67`), the transported-up fallbacks (`MoveUtils:220-233`), `ApplyUprightBias` fading
    (`MoveUtils:235-246`). A gravity slide on a ceiling would also hang still (gravity along the wall = 0).
21. **The slide's vertical model goes through three layers to do two lines.** `GenerateWallMove` → `ComputeVerticalSpeed`
    (base) → `ComputeAxisSpeed` with `Input = 0`, `MaxUp/Down = 0`, `StopSpeed = 0` (`Base:200-211`, `MoveUtils:76-109`) →
    add gravity (`Slide:38-48`) → `ClampVerticalSpeed`. Net effect: `v -= Friction*v*dt; v += g_along*scale*dt; clamp`.
22. `FGSTN_WallProbeResult::bValid` is set (`Query:310`, `Query:342`) and never read (`Base:411`, `Base:416` use the return
    value). `FGSTN_WallTickState::LastProbe` keeps the whole result to read two fields (`Base:557`). `EntryMomentum`,
    `FGSTN_WallDetach` (pull-away timer in the sync state, `PullAwayTime`), `FGSTN_WallInputs`, `SurfaceRelative` mapping and
    the base `MaxSubsteps` loop are all carried by a slide that uses none of them with its defaults.
23. The legacy-migration block in `GSTN_SimpleWallMovementMode.cpp:8-40` is unrelated to the slide but sits in the same family.

To be fair: the base's structure (plane / filtered normal / orientation / adhesion kept apart, `FGSTN_WallTickState`, base
riding through the rollback blackboard, the sync-state hygiene) is sound and is what v2 reuses. The problem is that one
pipeline serves climbing and sliding, so the slide carries climbing's probes, its corner anticipation and its fit tests.

## 2. Design: `UGSTN_WallSlideV2MovementMode`

### 2.1 Placement and A/B switch

- New class `UGSTN_WallSlideV2MovementMode : UGSTN_ControlledWallMovementMode` (`Public/WallMovement/Modes/`), not a child of
  the current slide: the current class stays untouched and can be renamed later without dragging v2 along. It reuses
  `FGSTN_WallSlideVertical` (same struct, same semantics) and re-implements the two vertical functions directly
  (`v -= Friction*v*dt; v += g_along*scale*dt; clamp`), finding 21.
- It overrides `SimulationTick_Implementation` and composes the base's protected steps it keeps (`MakeTickState`,
  `ApplyMovementBase`, `CheckForWallExit` from Controlled, `OrientToWall`, `UpdateMovementBase`, `CaptureFinalState`,
  `GetWallNormal`) with new stateless functions in `UGSTN_WallEdgeUtils`. No edit to the base, the current slide, the query
  utils or the movement utils; new functions are **added** to the utils only (additive, the current mode's behaviour is
  unchanged by construction).
- Registration: both modes on `SandboxCharacter_Mover`'s `Modes` map, `WallSliding` → current, `WallSlidingV2` → v2 (one
  shared-Blueprint edit, announced first, done by an idempotent script `Tools/GAS/setup_wall_slide_modes.py` with the usual
  `Saved/Backups/GAS` backup). The switch is `TargetModeName` of the Falling mode's `GSTN_AbilityWallEntryTransition`
  (`setup_wall_slide_modes.py --use v2|legacy`). `UGSTN_GA_WallSlide::WallModeNames` gets both names by default
  (`GSTN_GA_WallSlide.cpp:14`), so the ability needs no change per switch; the harness constant `'WallSliding'`
  (`gasnpp_test.py:339`, `:374`) becomes a `WALL_MODE` parameter. `gstn.Wall.Enter WallSlidingV2` already works.
- Later rename (owner's call, Q3): `UGSTN_WallSlideMovementMode` → `UGSTN_WallSlideLegacyMovementMode` with a `CoreRedirects`
  entry in `DefaultEngine.ini`, mode name `WallSlidingLegacy`; v2 takes the plain `WallSliding` name so GA, harness and
  transition defaults need no edit. Or keep both names forever. Nothing in v2 depends on the choice.

### 2.2 Tick pipeline (one pass, no substep loop)

`SimulationTick` per tick, in order (names are the new functions unless marked *reuse*):

1. `MakeTickState` *reuse*; `IsMovementLocked(StartState)` (H7, once it lands: locked ⇒ skip 3, no timers, still 2 and 6).
2. `ApplyMovementBase` *reuse* (T2: moving/rotating walls are the same seam), `TryMoveToFaceWall` *reuse*.
3. `CheckForWallExit` *reuse* (wall jump, detach).
4. **Look ahead** (`UGSTN_WallEdgeUtils::ProbeAhead`, 1-2 line traces): from the contact point, `LookaheadDistance = max(MinLookahead,
   |v| * LookaheadTime)` along the move direction `MoveDir = normalize(VectorPlaneProject(v, N))`, lifted off the wall by
   `h = MaxStepHeight + Margin` along `+N`, trace toward `-N` for `h + RecessDepth`:
   - hit at distance `t`: `Offset = t - h` (negative = bump of height `-Offset`, positive = recess), `Angle = acos(HitNormal · N)`;
   - no hit: the wall is gone ahead → convex edge; a second trace from the end point back toward the capsule (`-MoveDir`,
     length `LookaheadDistance`) returns the far face and its angle, or nothing (the wall simply ends).
   Each hit goes through `IsHitOnValidWall` so the T5 surface filter applies to it (a forbidden surface ahead counts as
   "wall ends"). Pawns are excluded by object type (finding 15; `FGSTN_WallEdgeSettings::bIgnorePawns = true`).
5. **Classify** (`ClassifyAhead`, pure): `None` | `Bump` (`|Offset| ≤ MaxStepHeight`, any angle: the surface is continuous within
   step height) | `Slope` (`Angle ≤ MaxSmoothAngle`) | `Concave` (`MaxSmoothAngle < Angle ≤ MaxConcaveAngle`, hit) | `Convex`
   (same band, from the second trace, or the wall ends) | `Blocked` (angle beyond the limits: a wall that cannot be turned onto).
   `Bump` wins over the angle classes: that is the "step-up has precedence" rule.
6. **Corner speed cap** (`EdgeSpeedCap`, pure): for `Concave`/`Convex` with the edge at distance `d` ahead and movement toward it,
   `CornerSpeed = TerminalDownSpeed * lerp(1, MinScale, (Angle - MaxSmoothAngle) / (90° - MaxSmoothAngle))` (`MinScale` per class,
   Q1), and the allowed speed now is `sqrt(CornerSpeed² + 2 * CornerBraking * d)`; the velocity is clamped to it along the plane.
   Stateless and substep-independent: recomputed from geometry every tick, it brakes on approach like a car before a turn and
   releases as soon as no edge is ahead. `Bump` and `Slope` never cap (the owner's rule).
7. **Move** (1 capsule sweep, `TrySafeMoveUpdatedComponent`). On a blocking hit: floor below (`IsFloorHit` *reuse*) and the
   classification is not `Bump` ⇒ `LeaveTo(FloorModeName)` as today; `Bump` ⇒ **step over** (`TryStepOverBump`: lift along `+N`
   by `BumpHeight + Margin` (sweep), redo the remaining move (sweep), adhesion pulls back in; 2 extra sweeps only when a bump is
   actually hit, finding 12-13); otherwise `TryMoveToSlideAlongSurface` *reuse* (the inside corner; the plane turns in step 8).
8. **Contact + adhesion in one sweep** (`AdhereAndSense`, new overload of `TryMoveToAdhereToWall` that returns the hit): the capsule
   sweep toward `-N` (reach `WallGap + WallProbeDistance`) is the contact probe: raw normal (`Hit.Normal`, rounded on edges as the
   spheres were), contact point, component (base candidate, T2), gap error. Pull-in by `≤ Hit.Distance - WallGap` is known free,
   so it is applied **unswept**; push-out stays swept. Nothing found ⇒ `FindWallAroundConvexEdge` *reuse* (2 sweeps, only when the
   wall is lost), then `NoWallGraceTime` ⇒ `NoWallModeName` as today.
9. `AttachToWall` *reuse* (`FilterWallNormal`: deadzone + rate limit, finding 14), `OrientToWall` *reuse* (0-1 overlap),
   `TransportVelocity` *reuse* (length-preserving; the cap in 6 is what removes speed at corners, so transport stays clean).
10. `UpdateMovementBase`, `CaptureFinalState` *reuse*.

Query budget: **2 shape sweeps + 1 line trace per tick on the common path** (move, contact/adhesion, lookahead), ≤ 4 with a
convex edge ahead or a converging rotation, ≤ 8 when stepping over a bump (vs 6 / 7-11 / ~24 per substep today). Target
in the acceptance: average ≤ 3.5, max ≤ 9 queries per tick on the A/B map, measured with the counter from W0.

No substep loop: at the slide's terminal speeds (400-800 cm/s) a 60 Hz tick moves 7-13 cm, a fifth of the capsule radius;
the move sweep cannot skip a concave wall and a convex edge is caught by adhesion + wrap. The limit becomes "≤ one capsule
radius per tick" (≈ 2000 cm/s at 60 Hz), enforced by clamping, not by substeps (Q6).

### 2.3 Data (small USTRUCTs, `GSTN_WallEdgeTypes.h`)

```
FGSTN_WallEdgeSettings   (Category "Wall|Edges")
  float MaxSmoothAngle = 15 deg      slope changes up to this are continuous: no cap
  float MaxStepHeight  = 10 cm       step up: bumps/ledges up to this are stepped over, never a corner, never a floor
  float MaxStepDownDepth = 10 cm     step down: a surface receding up to this is continuous wall, never an outside edge (6.13)
  float StepMargin     = 2 cm
  float LookaheadTime  = 0.15 s      LookaheadDistance = max(MinLookahead, |v| * LookaheadTime)
  float MinLookahead   = 20 cm
  float RecessDepth    = WallProbeDistance (reuse) how deep the ahead trace looks for a receding surface
  bool  bSlowOnConcave = true,  float ConcaveMinScale = 0.6   speed scale at a 90° inside corner
  bool  bSlowOnConvex  = true,  float ConvexMinScale  = 0.4   speed scale at a 90° outside edge
  float CornerBraking  = 2000 cm/s^2   approach deceleration (the cap curve)
  bool  bIgnorePawns   = true
FGSTN_WallAheadResult   (not reflected)  Kind, Angle, Offset, Distance, Normal, Point, Component
```
`MaxConcaveAngle` / `MaxConvexAngle` / `MinSurfaceAngle` / `WallGap` / `WallProbeDistance` / `NoWallGraceTime` / the normal filter
and orientation settings are the base's, unchanged. The v2 mode hides nothing; it just does not read `ProbeRadiusScale`,
`ProbeVerticalSpread`, `MaxProbeDeviationAngle`, `MinLookaheadDistance`/`LookaheadTime` (base), `MaxSubstepDistance`, `MaxSubsteps`
(documented in the class comment; a later `EditCondition` can grey them out without touching the base).

### 2.4 Sync state: no addition

The cap is applied in `SimulationTick` from the lookahead of the same tick, so `GenerateMove` keeps producing the pure slide
velocity and nothing has to cross a tick boundary. `FGSTN_WallMovementSyncState` is reused as is; reconciliation rules are
unchanged. If the owner later wants the slowdown as an impulse applied once per edge (Q4), that needs an "edge crossed" marker
in the sync state (one float, compared with a 5° tolerance); the cap design avoids it on purpose. Animation can read the edge
kind through a `BlueprintReadOnly` field written to the sync state later, with the same serialize/reconcile/interpolate care
as wall-movement.md "Data" demands; not in the first delivery.

### 2.5 Gravity, bases, filter, lock

- Gravity-relative: "down the wall" is `Plane.VerticalAxis` (gravity-up projected on the plane), bump height is measured along
  the wall normal, corner angle between normals; `IsFloorHit` uses gravity-up. No world Z anywhere, as the base.
- T2 moving surfaces: steps 2 and 10 are the base's; the contact sweep's component is the base candidate (finding 10 fixed by
  construction, one sweep names the base). The lookahead traces run after `ApplyMovementBase`, so they are in the moved frame.
- T5 surface filter: every accepted hit (contact, lookahead, far face, wrap) goes through `IsHitOnValidWall`, which is where the
  filter will live; v2 needs no edit when T5 lands.
- `IsMovementLocked` (H7): checked at step 1; locked ⇒ no exits, no timers, base riding and adhesion still run (the wall may move).

### 2.6 Extension points of v2

`virtual FGSTN_WallAheadResult ProbeAhead(...)`, `virtual EGSTN_WallEdgeKind ClassifyAhead(...)`, `virtual float ComputeEdgeSpeedCap(...)`,
`virtual bool TryStepOverBump(...)`. A future climb-v2 or a per-surface friction (`Slippery`, traversal-surfaces §11) overrides
these, never the tick.

## 3. Implementation plan W0..W6 (one checkpoint = one commit)

Ground rules as in plans/README.md §1: full build; Automation `GSTN.Wall.*` green; harness `scenario_jump_wall`,
`scenario_server_correction` on the wall, `force_reconcile_on_mode` (listen host + 2 clients, 60/15 ms, with and without 2 %
loss) clean for **both** mode names from W2 on; the current mode's behaviour byte-identical (W0 characterization tests).

| Id | Content | Files / classes | Tests and checks | Acceptance |
|---|---|---|---|---|
| **W0** instrumentation and characterization | per-tick query counter (`gstn.Wall.CountQueries`, counts sweeps/traces/overlaps through the existing `SCENE_QUERY_STAT` tags; logs avg/max per mode) and an edge metrics log (`gstn.Wall.Metrics`: time, mode, speed, filtered-normal angle change, queries) in `GSTN_WallMovementDebug.*`; pure tests of the shared math as it is | `Private/WallMovement/GSTN_WallMovementDebug.{h,cpp}`, `Private/WallMovement/Tests/GSTN_WallUtilsTests.cpp` | `GSTN.Wall.Utils.{FilterWallNormalDeadzoneAndRate, TransportVelocityKeepsSpeed, AxisSpeedBrakingOnly, IsFloorHitGravityRelative}`; baseline run of the current mode on the A/B map, numbers recorded in `Saved/Notes/wallslide-ab-baseline.md` | no behaviour change; counter and metrics work in PIE; baseline numbers for the current mode exist |
| **W1** edge data and pure math | `FGSTN_WallEdgeSettings`, `FGSTN_WallAheadResult`, `EGSTN_WallEdgeKind`; `UGSTN_WallEdgeUtils::{ClassifyAhead, EdgeSpeedCap, AllowedSpeedAtDistance, BumpHeightOf}` (no world) | `Public/WallMovement/GSTN_WallEdgeTypes.h`, `Public/WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h` + `.cpp` | `GSTN.Wall.Edge.{BumpWinsOverAngle, SlopeBelowSmoothAngleNoCap, ConcaveCapScalesWithAngle, ConvexCapScalesWithAngle, ApproachCurveMonotonic, CapNeverBelowCornerSpeed, GravityIndependentAngles}` | pure, deterministic, no engine world needed |
| **W2** the v2 mode, flat walls | `UGSTN_WallSlideV2MovementMode` (tick composed from base steps, vertical model inline), `UGSTN_WallEdgeUtils::ProbeAhead` (traces), `UGSTN_WallMovementUtils::AdhereAndSense` (additive overload returning the hit, unswept pull-in); registration script and GA default names; harness `WALL_MODE` | `Public/Private .../Modes/GSTN_WallSlideV2MovementMode.{h,cpp}`, `GSTN_WallEdgeUtils.cpp`, `GSTN_WallMovementUtils.{h,cpp}` (added function only), `GAS/GSTN_GA_WallSlide.cpp`, `Tools/GAS/setup_wall_slide_modes.py`, `Tools/GASNPPTest/gasnpp_test.py` | harness scenarios with `WALL_MODE='WallSlidingV2'`; `gstn.Wall.CountQueries` on the flat lane | slides, jumps, detaches like the current mode on flat walls; ≤ 3.5 queries/tick average on the flat lane; 0 unforced reconciles; current mode untouched (W0 tests green, baseline unchanged) |
| **W3** corners | cap from the lookahead (steps 4-6), convex far-face trace, wrap reuse, `Blocked` handling | same mode/utils files | A/B map lanes b/c (convex and concave 30/45/60/90/120°); metrics: speed before/at/after each edge; harness `scenario_jump_wall` on a corner lane | speed at a 90° edge ≈ `TerminalDownSpeed * MinScale` ± 10 %; no cap on 5°/10° slope changes; 0 unforced reconciles while crossing edges on clients |
| **W4** bumps and ledges | step-over (step 7), ledge-below rule (bump vs floor), recesses via unswept pull-in | same | A/B lanes d/e (bumps 2/5/10/15/25 cm, ledges below, recesses 5/15 cm) | bumps ≤ `MaxStepHeight` crossed without stopping and without leaving the mode; 15/25 cm ledges below still land (Walking); sideways speed loss on a 5 cm bump < 10 % |
| **W5** network and compatibility | moving-wall lane (T0 TS-A collider), `IsMovementLocked` pass-through (when H7 exists, else a stub test), `IsHitOnValidWall` on every hit (T5 ready), pawn exclusion | mode + harness | `scenario_server_correction` on the wall, `force_reconcile_on_mode('WallSlidingV2')`, moving wall at 150/300 cm/s with base rotation; two pawns sliding next to each other | ≤ 1 correction at mount/loss on a moving wall, 0 while riding; pawns never become walls; host 0 reconciles |
| **W6** A/B tooling and docs | metrics CSV export per run, `wall-movement.md` class map + this doc's status, owner decision recorded; optional **W7** rename to `_Legacy` (CoreRedirects, mode names, script, harness defaults) | docs, `Tools/`, `Config/DefaultEngine.ini` (W7) | all previous | owner can run the protocol below and read one table per mode |

Shared-asset edits that need the owner's OK before they happen: `SandboxCharacter_Mover` (W2: add the `WallSlidingV2` mode,
switch `TargetModeName`; W7: names), the A/B level `Content/Levels/L_WallSlideAB` (new, versioned), `DefaultEngine.ini` (W7).

## 4. A/B test protocol for the owner

Map `L_WallSlideAB` (static meshes, simple collision, one lane per row; every lane has a start marker at 4 m height so the
character grabs the wall while falling with `gstn.Wall.DebugInput "stick 0 1"` or a side stick):

| Lane | Geometry | What to look at |
|---|---|---|
| a | flat vertical wall, 10 m wide, 8 m tall | terminal speed reached, queries/tick, stability of the normal (no jitter), no reconciles |
| b | convex corners: pillars with 30/45/60/90/120° edges, slid across sideways while descending | speed before/at/after the edge; wall kept or lost; rotation smoothness |
| c | concave corners, same angles | same, plus whether the capsule gets wedged or oscillates in the corner |
| d | bumps on the wall: 2/5/10/15/25 cm deep, 30 cm tall, hit sideways and from above (ledge below) | crossed without stopping (≤ MaxStepHeight), landing on the 15/25 cm ones; no unwanted Walking exits on the small ones |
| e | recesses 5/15 cm deep | pull-in without pop; no wall loss |
| f | slope changes 5/10/15/25° (facets) | no slowdown at ≤ 15°, slowdown at 25° only if you want it (then lower `MaxSmoothAngle`) |
| g | curved walls, radius 100 and 300 cm (cylinders, inside and outside) | continuous turning, no cap triggered (a curve is a chain of small slopes), speed kept |
| h | moving wall (TS-A kinematic collider, 150/300 cm/s, with 20°/s rotation) | riding, exit velocity includes the base, corrections at mount/loss only |
| i | overhang 100°/110° and a 60° slanted wall | slide slower on the slant, grab/let go rules unchanged |

Per run (both modes, same inputs through `gstn.Wall.DebugInput`, `gstn.Wall.Launch` for a fixed entry velocity): the
`gstn.Wall.Metrics` CSV gives speed at each edge crossing (before/at/after), time to cross, queries per tick (avg/max), wall
losses (`NO WALL` lines), mode exits; the harness gives reconciles per client. Subjective column: feel at corners (1-5), feel on
bumps (1-5), any visible snap. Decision inputs: lanes b/c/d are the owner's request; a/g are the regression guard.

## 5. Domande per il proprietario (con default consigliato)

1. **Rallentare su entrambi i tipi di spigolo?** Convessi (esterni, es. l'angolo di un pilastro) e concavi (interni) hanno scale
   separate. *Default*: entrambi, `ConvexMinScale = 0.4`, `ConcaveMinScale = 0.6` a 90°, lineare da `MaxSmoothAngle` in su.
2. **Soglie "piccolo"**: rilievo fino a `MaxStepHeight = 10 cm` (si scavalca, mai rallenta, mai Walking) e cambio di scoscesità
   fino a `MaxSmoothAngle = 15°` (nessun rallentamento). *Default*: questi valori, entrambi tunabili nel `FGSTN_WallEdgeSettings`.
3. **Nomi e rinomina**: v2 registrata ora come `WallSlidingV2` accanto a `WallSliding`; dopo i test, rinominare la corrente in
   `UGSTN_WallSlideLegacyMovementMode` / `WallSlidingLegacy` (CoreRedirects) e dare a v2 il nome `WallSliding`, così GA, harness e
   transizione non cambiano. v2 deriva da `UGSTN_ControlledWallMovementMode`, non dalla slide corrente, proprio per rendere la
   rinomina indolore. *Default*: sì a entrambe le cose, rinomina solo dopo la tua decisione.
4. **Forma del rallentamento**: un tetto di velocità calcolato ogni tick dalla geometria davanti (frenata progressiva prima dello
   spigolo, nessuno stato aggiuntivo nel sync state), oppure una perdita secca una volta sola al passaggio (richiede un marker
   nel sync state)? *Default*: il tetto; l'impulso resta possibile come override di `ComputeEdgeSpeedCap`.
5. **Pedoni come muri** (finding 15): v2 ignora le capsule degli altri pawn nelle sue query. Vuoi la stessa regola anche nella
   modalità corrente e nella transizione di ingresso (è un edit al core, stessa classe di `IsMovementLocked`)? *Default*: solo in
   v2 adesso; il core dopo i test, in un commit separato.
6. **Niente substep in v2**: un solo sweep per tick, limite implicito "un raggio di capsula per tick" (≈ 2000 cm/s a 60 Hz,
   ben oltre i 400-800 della slide). Accetti il limite al posto del loop di substep? *Default*: sì.

## 6. Owner direction of 2026-10-24 and the resulting design (supersedes §2 where they differ)

Owner, 2026-10-24 (verbatim): "il wallslide dovrebbe mantenere il momentum in entrata, avere supporto alle basi di movimento,
avere poco movimento di base (tipo mario), ma grande capacità di frenata. Inoltre vorrei che fosse intercambiabile con una modalità
di climbing con parametri aggiustabili su alcune superfici dedicate che consentono il climbing vero. Hopefully, dovrebbero
praticamente avere la logica di base in comune, ed essere estendibili tramite proposed move e quant altro. [...] il ragionamento
degli sviluppatori di warframe: la geometria dei loro tileset è troppo complessa, e quindi non possono introdurre un wall run
fluido. Il wallslide dell'ultima volta rallenta, si intoppa, e il modo in cui avviene il girarsi causa l'inceppamento su piccoli
dislivelli o geometria complessa. Vorrei un modo per evitare ciò."

### 6.1 Why the current slide snags (the Warframe problem), and the rule that removes it

1. **The collision shape turns with the wall.** `ComputeTargetUp`/`OrientToWall` tilt and yaw the capsule toward each new normal;
   a capsule rotated into complex geometry overlaps it, the orientation step needs overlap tests and pushes, and a small ledge or
   facet makes it jam or pop. **Rule: the collision capsule never rotates on a wall.** Its axis stays gravity-up (a capsule is
   symmetric about that axis, so facing the wall needs no collision rotation at all); facing is presentation (mesh/actor yaw for
   animation), written to the sync state as a yaw only. Climbing overhangs, where tilting is unavoidable, is the climb mode's
   opt-in, never the slide's.
2. **Motion follows the raw surface.** Every facet normal changes the movement plane and the velocity transport, so a ribbed or
   tiled wall turns into dozens of tiny corners, each braking or deflecting. **Rule: the character moves on a proxy plane**, not on
   the raw surface: the proxy normal is the contact sweep's normal filtered (deadzone + rate limit, as today) **and** stabilised by
   the lookahead (the classification of §2.2 step 5: `Bump` and `Slope` never change the proxy's plane abruptly, only real
   `Concave`/`Convex` edges do, and then through a rate-limited turn). Small features are absorbed by adhesion (the capsule stays at
   `WallGap` from the real surface, pulled in unswept, pushed out swept) and by step-over along the wall normal, never by a turn.
3. **Speed is lost in many places.** Today friction, axis braking, transport and substep slides each take a share. **Rule: one
   place decides speed** (the control model, §6.2); geometry may only cap speed ahead of real corners (§2.2 step 6), transport is
   length-preserving, and a blocked sweep slides along the surface without a separate friction term.

### 6.2 Control model (slide): momentum in, little control, strong braking

Velocity on the wall is split into the along-gravity axis `V` and the horizontal-on-wall axis `H` of the proxy plane.

- **Entry**: the falling velocity is projected onto the proxy plane at grab time, keeping its length (`EntrySpeedKeep = 1.0`,
  tunable, with `MaxEntrySpeed` clamp): you keep the horizontal run and the vertical speed you had. Nothing is zeroed.
- **Horizontal `H`, Mario style**: momentum decays slowly (`HorizontalDrag`, low); stick input along `H` adds only a small
  acceleration (`HorizontalControlAccel`, low, and capped at `HorizontalControlMaxSpeed`); stick **against** the current `H`
  brakes hard (`HorizontalBrakeDecel`, high).
- **Vertical `V`**: gravity along the wall scaled (`GravityScale`) toward a terminal speed (`TerminalDownSpeed`) with a soft
  approach; upward entry speed decays under gravity (no instant stop). Stick **down** (toward gravity) is "let go faster"
  (`DownBoost`, optional); stick **up** or a dedicated brake button brakes hard (`VerticalBrakeDecel`, high) down to
  `BrakeMinSpeed`. Which input brakes is data (`FGSTN_WallBrakeInput`: stick-against-motion, stick-up, an input tag), so
  the input layer and the GA can decide.
- All of it is a pure function `UGSTN_WallControlUtils::StepSlideVelocity(Settings, VelocityOnPlane, InputOnPlane, Brake, Dt)`,
  testable without a world, and called from `GenerateMove` (it builds the `FProposedMove`), not from the tick.

### 6.3 One core, two modes (slide and climb), extended through the proposed move

- `UGSTN_WallTraverseModeBase` (new, abstract): the lean pipeline of §2.2 (base riding, exit checks, lookahead/classify, cap,
  single move sweep, step-over, contact+adhesion in one sweep, proxy plane, presentation yaw, capture) — no control model of its own.
- `GenerateMove` builds the `FProposedMove` from a virtual `ComputeWallVelocity(StartState, Plane, Input, Dt)`; Mover's layered
  moves and queued moves (abilities, knockback, launches) combine with it as for any mode, and the tick moves by the **final**
  proposed velocity. Extension = override `ComputeWallVelocity` (or the edge hooks of §2.6), or add a layered move; never the tick.
- `UGSTN_WallSlideV2MovementMode`: `ComputeWallVelocity` = §6.2. `UGSTN_WallClimbV2MovementMode`: full 2D stick control on the
  proxy plane (`ClimbSpeed`, `ClimbAccel`, `ClimbBrake`, optional stamina hook as a tag query, no gravity slide while holding),
  allowed only on surfaces whose `Surface.Behaviour` has `WallClimb` (T5 filter on the mode and on its entry transition), with
  `bAllowCapsuleTilt` (off by default) for overhangs. Both share every tunable struct except their control settings.
- **Interchangeable**: the entry transition resolves the hit's surface (`UGSTN_SurfaceUtils::Resolve`) and picks the climb mode
  when the surface offers `WallClimb` and the climb mode is registered, else the slide (a priority list of `{TagQuery, ModeName}`
  on the transition, data). While on a wall, crossing from a climb-only surface onto a slide-only one switches mode in place with
  the velocity kept (same core, same sync state).

### 6.4 Plan update

W0-W1 unchanged (instrumentation, pure edge math) plus `UGSTN_WallControlUtils` and its tests in W1. W2 builds the shared base and
the slide on it; W3-W4 unchanged (corners, bumps); **W4b** the climb mode and the surface-driven transition; W5-W6 unchanged. The
current modes stay as they are for the A/B test.

### 6.5 W2 status (code only, 2026-10-24)

Built: `UGSTN_WallTraverseModeBase` and `UGSTN_WallSlideV2MovementMode` (derived from `UGSTN_ControlledWallMovementMode`, so the legacy modes are untouched),
`UGSTN_WallMovementUtils::AdhereAndSense`, `Tools/GAS/setup_wall_slide_modes.py` (written, NOT run: it edits `SandboxCharacter_Mover`), `WALL_MODE` in the harness.
The edge hooks of 2.6 (`ProbeAhead`, `ClassifyAhead`, `ComputeEdgeSpeedCap`, `TryStepOverBump`) are declared and wired into the tick with no-op defaults, and
`Edges` (`FGSTN_WallEdgeSettings`) is stored but inert until W3/W4. Facing is written as the capsule's yaw about gravity-up (the default sync state's orientation),
no new sync-state field. Not yet done: pawns excluded from the contact sweep (W5), `IsMovementLocked` (H7), `IsHitOnValidWall` on the ahead traces (W3).
Needed outside this checkpoint: `UGSTN_GA_WallSlide::WallModeNames` (`GAS/GSTN_GA_WallSlide.cpp:14`) must also list `WallSlidingV2`, otherwise the ability ends at once
on the v2 mode; it was left unchanged (GAS territory).


### 6.6 W3 and W4 status (code, 2026-10-24)

Built in `UGSTN_WallTraverseModeBase` and `UGSTN_WallEdgeUtils` (tests `GSTN.Wall.V2.*` in `GSTN_WallV2EdgeTests.cpp`, pure ones in `GSTN_WallEdgeTests.cpp`).
Not yet run in the editor (the owner's editor was open): compile-checked on the game target only.

- **Look-ahead** (`ProbeAhead`): the capsule is taken as seated on the wall plane at `WallGap`, the plane is read at `LookaheadDistance` past the capsule's
  *leading edge* (`CapsuleSupportAlong`, so a slide down looks from the feet, not from the centre). Trace 1 (always, the only one on a flat wall): lifted
  `MaxStepHeight + StepMargin` off the plane at the end point, down toward the wall, `RecessDepth` deep (the v2 slide sets it to `WallProbeDistance`).
  Trace 2 only if trace 1 starts inside geometry (something taller than the step: an inside corner): a ray along the movement from the lifted leading edge
  gives the face, angle and distance. Trace 3 only if the wall ends: a ray back from just behind the plane gives the far face of an outside edge (no face: a
  90 degree edge at half the look). Every accepted hit goes through `IsHitOnValidWall`; a refused surface counts as "the wall ends"; pawns are ignored by
  response (`bIgnorePawns`). This replaces the single-trace description of 2.2 step 4, which could not see an inside corner (its end point is inside the corner).
- **Classification refinements** (`ClassifyAhead`): a face within the smooth angle is a `Slope` only if its offset is what such a slope makes over the sampled
  distance, otherwise it is a step (`Bump`): a ledge face is parallel to the wall. Past step height the sign of the offset decides: rising = `Concave`,
  receding = `Convex`.
- **Cap**: `ComputeEdgeSpeedCap` = corner speed (`GetEdgeReferenceSpeed()`, the slide's `TerminalDownSpeed`, times the angle scale) raised along the approach
  curve by the distance to the edge; it clamps the proposed velocity in `TraverseStep`, transport stays length preserving. `Bump`, `Slope`, `Blocked`: no cap.
- **Proxy plane**: the contact normal turns it only if the look-ahead said `Concave`/`Convex` this tick, the move met an inside corner, or the contact normal is
  within `MaxSmoothAngle` of the plane (curves, facets); the wrap around a convex edge (unchanged, two sweeps only when the wall is lost) always may.
- **Blocked**: never capped, never a reason to turn, never stepped; the move sweep then slides along it or, for a floor, leaves to `FloorModeName`.
- **Step-over** (`TryStepOverBump`, tried on every blocking hit): a line trace at the hit (`ProbeBumpAt`) sizes the feature; if it is a `Bump` (up to
  `MaxStepHeight`) and the look-ahead did not name an inside corner, the capsule is lifted along the wall normal by `StepLiftFor` (height above the seat plus
  `StepMargin`, swept), the rest of the move is redone (swept), adhesion pulls back in. Anything that does not clear is undone. A ledge below up to the step
  height is therefore stepped (never `FloorModeName`); taller ones fall through to the floor rule. A recess is absorbed by the rate-limited unswept pull-in; a
  recess deeper than `MaxStepHeight` that the capsule has dived into is exited as a corner, not as a bump.
- Known limits for PIE: a curved wall (lane g) shows a growing angle at the end of the look; since 6.12 a few extra samples tell an arc (steps of at most `MaxSmoothAngle`) from a crease and an arc is a `Slope`; a recess
  deeper than `RecessDepth` is read as an outside edge; features narrower than the look slip between two ticks and are met by the move sweep (then step-over).

### 6.7 Stick into the wall (owner PIE report, 2026-10-24)

Facing the wall, "stick forward" was mapped by the camera-relative mapping onto the plane's up axis and read as the vertical brake (stick against the fall),
which held the slide at `BrakeMinSpeed`. In v2 `MapMoveIntentToWall` drops the vertical part of a stick that mostly pushes into the wall (more than half of its
size, `UGSTN_WallControlUtils::IntoWallAmount` / `SuppressVerticalIntoWall`); the into-wall component is otherwise ignored, so pushing toward the wall does nothing special.
Option kept, off by default: `HoldTowardWallTerminalScale` (1 = off) scales the terminal down speed with the push. `BrakeMinSpeed` is 80 cm/s and a held brake brings any
slower or upward speed up to it, so the slide cannot freeze. Tests: `GSTN.Wall.Control.{PushIntoWallDoesNotChangeSlide, BrakeNeverBelowMinSpeed}`, `GSTN.Wall.V2.PushIntoWallDoesNotChangeSlide`.

### 6.8 Inclined walls and overhangs (owner request, 2026-10-24)

"Climbing is the other way round: the wall slide must also work on more or less inclined walls." Angles here are the angle between the surface normal and gravity-up
(`Beta`): 90 is vertical, 60-85 a wall leaning back (normal tilts up, the upright capsule touches with its lower hemisphere), 95-120 an overhang (normal tilts down, upper hemisphere).

- **Accepted range: 45 to 120 degrees**, for the slide v2 mode (`MinSurfaceAngle` / `MaxSurfaceAngle`) and, now, for the entry transition (its default minimum was 60, which
  excluded 45-60 and put the usual 60 degree slant on a float knife edge). Below 45 is a floor (Mover's walkable limit), the floor rule (`IsFloorHit`, gravity-relative) is unchanged and does not
  fire on 50-70 degree slants. A saved transition asset that overrides `MinSurfaceAngle` keeps its value.
- **Entry confirmation trace**: it ran one radius horizontally from the capsule center, which never reaches a surface leaning back 60 degrees (the center is `support` away along the normal, 61 cm for the
  default capsule, 70 cm horizontally). It now runs along the face normal for the capsule's extent along it (`+ ProbeDistance + 5`); identical on a vertical wall.
- **Down the wall** is gravity projected on the proxy plane (`UGSTN_WallControlUtils::AdaptToSlope`): the gravity along the plane is `|g| sin(Beta)`, and the terminal down speed and `DownBoost` follow it
  (`Control.SlopeTerminalScaling`, 1 = fully, 0 = off). An 60 degree wall slides at 87% of the vertical speed over the whole run, an 80 degree one at 98%. Overhangs use the same law
  (`sin(110) = 0.94`); the velocity is always on the plane, so gravity pulling the character off an overhang is held only by the adhesion (`MaxAdhesionSpeed`), as on any wall.
- **Inside corner fit test** (`CanFitOnWall`, `bKeepOrientation`): the legacy test lays a rotated capsule on the new wall; the v2 capsule stays upright, so the pose is the current orientation seated
  by its own extent along the new normal. **Wrap around an outside edge** (`FindWallAroundConvexEdge`, `WallSupport`) dives from that extent instead of the radius. Both default to the legacy behaviour.
- Already gravity-relative and unchanged: adhesion (a capsule sweep along the normal, so the gap is the hemisphere's), the look-ahead (`CapsuleSupportAlong`), corner and bump classification (angles
  between normals, heights along the normal), the facing yaw.
- Known limit: near a floor the adhesion sweep (along -N, which has a downward part on a slant) can hit the floor before the wall; that reads as "no wall" for the last ~1-2 ticks before the floor rule
  takes over (grace time 0.15 s). Tests `GSTN.Wall.V2.{SlantedWallSlidesDown, OverhangKeepsContact, SlantSlowerThanVertical, SlantNotTreatedAsFloor, EntryAcceptsInclinedWalls, CornerOnSlantedWallCaps,
  BumpOnSlantedWallSteppedOver, AdhesionGapOnTiltedContact, WrapFindsFaceAroundEdgeOnSlant}` (`GSTN_WallV2SlantTests.cpp`); written with the editor open, so not yet run.

### 6.9 W4b status: the climb mode and the surface-driven choice (2026-10-25)

Built: `UGSTN_WallClimbV2MovementMode` (`ComputeWallVelocity` = `StepClimbVelocity`, `FGSTN_WallClimbControlSettings` incl. entry keep/clamp, filter default `Surface.Behaviour.WallClimb`
with unmarked surfaces refused, `bAllowCapsuleTilt` declared and inert: the capsule never rotates in v2), the priority list `FGSTN_WallSurfaceModeList` (`GSTN_WallSurfaceModeTypes.h`:
`{Behaviours, ModeName, BlockedWhen}` entries, default WallClimb -> `WallClimbingV2`, WallSlide -> `WallSlidingV2`, unmarked surfaces -> the slide, 6 ticks of hysteresis), its pure
functions (`UGSTN_WallSurfaceModeUtils`: `PickMode`, `StepVote`, `ChooseModeForHit`), and two entry transitions that choose the mode by the hit's surface:
`UGSTN_WallSurfaceModeTransition` (plain) and `UGSTN_AbilityWallSurfaceModeTransition` (the same behind the ability check; the sandbox character's Falling transition is the ability one).

- **Unmarked surfaces are slide walls.** The plugin's catch-all default offers WallClimb too, so reading its behaviours would turn every unauthored wall into a climb wall; `PickMode` treats
  `Source == Default` apart (`UnknownSurfaceModeName`). Climbing is authored with a `UGSTN_SurfaceBehaviourComponent` offering `WallClimb`.
- **In-place switch** (`UGSTN_WallTraverseModeBase::UpdateSurfaceVote`, after the world part of the tick): the contact of the adhesion sweep is resolved, `PickMode` names the mode (registered and
  not blocked), and when it is not this mode for `HysteresisTicks` consecutive ticks the mode `LeaveTo`s it with no time given back. The counter is `FGSTN_WallMovementSyncState::SurfaceSwitchTicks`
  (rolled back with the state; sent only when non-zero; not compared by `ShouldReconcile`, the mode name is). `Activate` recognises the hand-over (`IsInPlaceSwitchFrom`) and leaves the sync state,
  velocity, wall and base untouched. While the list is enabled the probes accept every surface any available listed mode takes (`FGSTN_WallSurfaceFilter::AlternativeBehaviours`, filled by the
  overridden `GetWallCheckSettings`); the mode's own `SurfaceFilter` applies when the list is disabled or only one of its modes is registered.
- **Stamina hook** is data: `BlockedWhen` on a list entry is a tag query over the Mover tags of the tick's start state. A blocked climb is skipped; a surface whose only matches are blocked sends a
  character on a wall to `BlockedModeName` (Falling) and refuses the entry. No GAS in the mode: an ability grants the tag.
- Changes outside the v2 files, all additive and default-off for the legacy modes: `UGSTN_WallMovementModeBase::GetWallCheckSettings` virtual; `FGSTN_WallSurfaceFilter::AlternativeBehaviours` (empty = old
  behaviour); `FGSTN_WallTickState` contact and vote fields and the sync state's `SurfaceSwitchTicks` (always 0 for legacy modes; one more flag bit on the wire); `UGSTN_WallEntryTransition::ResolveEntryMode`
  (returns `TargetModeName`) and the candidate's hit; `UGSTN_GA_WallSlide::WallModeNames` lists `WallClimbingV2`.
- Setup: `Tools/GAS/setup_wall_slide_modes.py --use v2climb` registers the climb mode and swaps the Falling entry transition for its surface-driven twin (`v2` / `legacy` swap it back). Tests
  `GSTN.Wall.V2Climb.*` (`GSTN_WallClimbV2Tests.cpp`) and the sync state round trip. Not covered headless: `Activate`'s in-place path and the transitions' `Evaluate` (PIE).

### 6.10 W5 status: pawns, the movement lock and moving walls (2026-10-25)

Code, all additive (the legacy modes' behaviour is unchanged by construction: the new switches default to off for them).

- **Pawns are never walls in v2** (finding 15). `FGSTN_WallCheckSettings::bIgnorePawns` (default false) is filled from `FGSTN_WallEdgeSettings::bIgnorePawns` (default true) by the traverse base's `GetWallCheckSettings`.
  It does three things: the contact sweep (`AdhereAndSense`), the corner / wrap queries (`FQueryContext`) and the look-ahead / step probes (`FProbeContext`, already) ignore the Pawn channel in their response params, so a character between the capsule
  and the real wall does not hide it; and `UGSTN_WallQueryUtils::IsHitOnValidWall`, where every acceptance ends, refuses a hit on a Pawn-type component, so the move sweep's blocking hit on a neighbour is never read as an inside corner.
  The pawn still blocks the move itself (ordinary collision) and a landing on a pawn's head is still a floor. Not covered: the entry transition (core, owner's Q5: separate commit after the tests) and `CanFitOnWall`'s overlap test, which still counts a pawn as an obstacle.
- **`IsMovementLocked`** is a protected virtual of `UGSTN_WallTraverseModeBase`, because the base on this branch does not have the H7 one (`48c2a99` on `feature/gas-npp-5.8`, not merged here). Same signature and the same default as H7
  (`Combat.Mover.Frozen` in the start sync state, looked up by name): when the branches meet, this declaration and the base's are one override point and the traverse one can be deleted. Locked tick: `CheckForWallExit` skipped (no wall jump, no detach),
  no floor exit, no no-wall exit and `TimeWithoutWall` standing still, no surface-mode vote or switch; base riding, the move, adhesion and the plane filter still run (the wall may move under a frozen character). The flag is the tick-local `FGSTN_WallTickState::bMovementLocked`.
- **Moving walls** use the legacy base path unchanged (`ApplyMovementBase` before the exits, `UpdateMovementBase` after, the ridden wall in the rollback blackboard); the v2 contact sweep names the base through `LastProbe.Component`. The exit adds
  `GetPhysicsLinearVelocityAtPoint` of the base (`bAddBaseVelocityOnExit`).
- Tests `GSTN.Wall.V2Net.{PawnsNeverWalls, LockedSkipsExitsKeepsAdhesion, RidesMovingWall, ExitVelocityIncludesBase, SyncStateRoundTripWithSurfaceSwitch}` (`GSTN_WallV2NetTests.cpp`); every one has a control that must show the opposite behaviour (legacy switch,
  unlocked, riding off). The base tests drive a real rollback blackboard frame by frame. The kinematic wall is teleported (physics) between ticks, as the physics step would have left it; the exit-velocity test uses a gravity-less simulated body so the headless world reports a velocity at once.
  A kinematic TS-A collider moved by actor location gets its velocity from the physics step (Chaos kinematic target): the PIE lane h is what proves that path.
- **PIE harness, `WALL_MODE = 'WallSlidingV2'` (listen host + 2 clients, 60/15 ms, editor started headless by a driver, 2026-10-25).** Client 1 grabs the harness wall three times per run (ground jump, jump at the wall, wall jump), slides to the floor each time (about 1.3 s per grab);
  host, client 1 and client 2 show the same mode timeline within the latency. During the first grab a server-only dash and buff are applied to the pawn on the wall, during the second `force_reconcile_on_mode(1, 'WallSlidingV2')` forces a reconcile in the mode (it reconciled: rollbacks 2 -> 4).
  Result, at 0 % and at 2 % loss alike: client 1 had exactly the reconciles those three events cause (the server dash: one ASC plus one Mover mismatch on the same frame; the buff: one ASC mismatch; the forced one) and none of its own; client 2 (idle) and the host 0; `report()` CMP 6/6 both times.
  Not run: a moving wall (the harness map has no kinematic collider) and two pawns on the wall together; the headless tests cover both. How it was driven: `UnrealEditor.exe <uproject> /Game/Levels/DefaultLevel_Harness -ExecCmds="py <driver>.py" -unattended -nullrhi -abslog=<log>` with `PlayNumberOfClients=3`
  (listen, one process) in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, the driver being a state machine on `register_slate_post_tick_callback` that calls `setup_editor(True)`, `LevelEditorSubsystem.editor_request_begin_play()`, `np.PrintReconciles 1`, the scenarios and `QUIT_EDITOR`.
  `-ExecutePythonScript` is no use for this: with `-unattended` the editor quits as soon as the script returns, before the first tick callback. Editor not touched afterwards, the ini restored.

### 6.11 W6 status: A/B tooling (2026-10-25)

Built, all additive (the legacy modes' behaviour is unchanged: the metrics hook at the end of the legacy tick only feeds the CSV).

- **Metrics CSV.** `gstn.Wall.Metrics 1` starts a run and `0` closes it; one file per run and mode, `Saved/Metrics/wallslide-<mode>-<yyyymmdd-HHMMSS>.csv` (written when the run is closed, at 16 KB of buffer and at engine exit).
  Columns: `time,mode,speed,hspeed,vspeed,dnormal_deg,queries,edge_ahead,events,x,y,z,source` (speed in cm/s, `vspeed` along the up axis, `dnormal_deg` = filtered normal change this tick, `queries` as `gstn.Wall.CountQueries`,
  `edge_ahead` = kind of the look-ahead of the tick (v2 only, `none` for the legacy slide), `events` = `grab|exit|wall-lost|mode-switch`, `source` = `<pawn>@<client|listen|server|standalone>` because a PIE host and its clients share one sink).
  `grab` is derived by the sink (first tick of a source in a mode, a gap over 0.1 s, or a mode change; a re-simulated tick is never a grab); `wall-lost` is the first tick without contact; `mode-switch` is the surface vote.
  The pure formatter (`GSTNWallDebug::FormatMetricsRow`, `IsGrabTick`) has `GSTN.Wall.Debug.MetricsCsv`. The legacy tick now calls `FinishTick` too (debug builds only), so both modes produce rows; it is not query-instrumented (queries read 0, the summary prints `-`).
  New console command `gstn.Wall.Teleport X Y Z [Yaw] [PlayerId]` (Mover teleport + stop) and an optional PlayerId on `gstn.Wall.Launch`, so a harness moves one player's pawn (the command reaches every world: both copies of the pawn get the effect).
- **Moving wall actor** `AGSTN_WallTestMover` (`Public/WallMovement/Debug/`, kept next to the wall code because `GASNPPTest/` holds the harness's ability content, not map content): a Movable cube that ping-pongs `TranslationSpeed` over +-`TravelDistance` along `TravelAxis` and turns +-`YawRange`
  at `YawRate`, the pose a pure function of `UWorld::GetTimeSeconds` and of the BeginPlay pose (triangle waves; `GSTN.Wall.TestMover.Pose`). A constant 20 deg/s yaw would turn the wall away from the character for good, hence the back and forth.
- **Map** `Content/Levels/L_WallSlideAB.umap`, built by `Tools/GAS/build_wall_slide_ab_map.py` (idempotent: the lane table is hashed into a tag on a `Spec` actor, a run that finds the same hash does nothing; headless recipe in the script header). Lanes along +Y, 12 m apart, wall face on the plane x = 0:
  a flat, b convex 30/45/60/90/120, c concave 30/45/60/90/120, d bump 2/5/10/15/25 cm (a rib crossed sideways and a 30 cm ledge landed on, second marker `_down` over the ledge), e recess 5/15 cm, f facets 5/10/15/25 deg,
  g curves r100/r300 outside and inside (chains of 10 deg chords), h moving wall 150/300 cm/s with 20 deg/s yaw, i slant 60 and overhang 100/110, j a climb-only wall and a wall that offers neither slide nor climb (`UGSTN_SurfaceBehaviourComponent`).
  Plain engine cubes as thin plates behind each face (only the face is in play), default material, one sun and sky light. 38 `TargetPoint` markers tagged `WallABLane`, `lane:<id>`, `group:<x>`, `launch:<x,y,z>`, `stick:<r,f>` at 4 m height, 50 cm from the wall; the world settings carry `GM_GASNPPTest`.
- **Driver** `Tools/GASNPPTest/wall_ab.py`: for every lane and for `WallSliding` / `WallSlidingV2` it sets the entry mode of the pawn's Falling transitions in memory (assets untouched; surface-driven transitions through `UnknownSurfaceModeName`), teleports the client's pawn to the marker,
  launches it (`gstn.Wall.Launch`), sets the stick (`gstn.Wall.DebugInput`: into the wall plus a side part, because the grab needs intent into the wall), records for `--seconds`, reads the CSV and the reconcile lines, and writes `Saved/Metrics/wallslide-ab-summary.md`. The host's own pawn is parked.

**How the owner runs the A/B.** Editor closed: `python Tools/GASNPPTest/wall_ab.py` (all lanes, both modes, about 10 minutes; `--lanes "^b_"` for a regex on lane ids, `--modes WallSlidingV2`, `--seconds`, `--lag/--var/--loss`). It starts a headless editor on the map (listen host + 1 client, one process, ini patched for the run and restored)
and prints the table. For the feel (the subjective columns of section 4) open `L_WallSlideAB` in the editor, play it with the sandbox character, and use `gstn.Wall.Metrics 1`, `gstn.Wall.CountQueries 1`, `gstn.Wall.QueryStats` by hand; the CSV lands in `Saved/Metrics`. Rebuild the map after a change of the lane table with
`UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/GAS/build_wall_slide_ab_map.py -unattended -nullrhi -nosplash -nosound -NoP4`.

**First full run (2026-10-25, listen host + 1 client, 60/15 ms, 0 % loss, 3.5 s per run, all 38 lanes, both modes; table in `Saved/Metrics/wallslide-ab-summary.md`, not versioned).** What it says, and what it does not:

- Grabs on every lane except the two surface lanes. Query counter of v2: 3.1 to 4.0 queries per tick on average (max 4 to 11); the legacy slide is not instrumented.
- Corner speed ("before / at" in cm/s, v2 vs legacy): convex 30 deg 501 / 238 vs 434 / 472; convex 90 deg 501 / 154 vs 434 / 387; concave 90 deg 501 / 112 vs 435 / 387; facets 5 to 15 deg and bumps 2 to 15 cm: 501 / 350 (no cap beyond the flat-wall value of 350) vs 434 / 472.
  The v2 cap works at corners and leaves small slope changes and bumps alone; the legacy slide keeps its speed through every corner.
- Reconciles on the client are 3 to 5 in the first 0.6 s of every run, 0 afterwards (the teleport and the launch go to both copies of the pawn: a harness artefact, equal for both modes); the moving-wall lanes keep reconciling while riding (legacy 37 to 50 after 0.6 s, v2 4 to 16): the wall pose is a function of each machine's own `GetTimeSeconds`, which differ by the connection latency,
  so a ridden wall is a prediction error source in this harness. A mover for production levels should be driven by a replicated pose.
- Not answered by this run: the "after" and "cross" columns are empty for v2 because at 4 m height and 3 m to the feature the capped character reaches the floor first (lower `FEATURE_DY` or raise the start in the builder to see past the edge); the climb-only and no-slide lanes show no grab (see below).
- Open: `j_climb_only` (the surface-driven entry should hand a WallClimb surface to `WallClimbingV2`) and `j_no_slide` did not grab at all in the harness, so the climb entry is unconfirmed here (the unit tests cover the choice, PIE by hand is needed); the v2 look-ahead reports `slope` / `bump` on the first ticks of the flat lane (the wall start is part of what it sees).

### 6.12 Findings of the first A/B run, and what was done (2026-10-25)

The first run (6.11) listed five problems. What each one was:

1. **"v2 slows from 501 to 350 cm/s on lanes with no cap".** Not a cap. The trajectories of the bump, recess, facet and curve lanes were bit identical to the flat lane up to the feature; the 350 was the floor-exit tick (the character, started at 4 m, reached the floor right at the feature, 3 m along the lane, and the exit drops the vertical speed), which the "at" window picked up as its minimum. The harness now leaves the leaving tick out of the windows, and the lanes start at 10 m (`START_HEIGHT` 1000, walls 12 m) so the "after" and "cross" columns fill. Headless lane tests (`GSTN.Wall.V2.Lane.*`, `GSTN_WallV2EdgeTests.cpp`) slide a capsule diagonally across each feature with a constant proposed speed (350 along and 450 down, 500 / 100, 450 / 540; four gravity frames) and demand that the tick takes nothing away. They found three real faults in the look-ahead, all fixed in `UGSTN_WallEdgeUtils::ProbeAhead`:
   - **A curve was read as a corner.** A r300 wall of 10 degree chords showed 20-30 degrees at the end of the look and capped the slide to the corner speed (a 5 to 20 % dip, more with the wall turning under a fixed stick). One angle cannot tell a crease from an arc; the candidate corner is now sampled from the capsule to the end of the look (five steps: down traces on the convex side, lifted rays on the concave side) and only a single step of more than `MaxSmoothAngle` between two samples is a corner. A chain of small steps is a `Slope` (no cap; the plane follows it through the gentle-normal rule). Only for turns up to 3 x `MaxSmoothAngle` within the look (a sharper turn is a rounded corner and keeps the cap), at the cost of up to five extra traces while such a corner is in the look.
   - **A receding facet was read as the end of the wall.** The down trace reached `RecessDepth` below the plane; a 15 degree facet is 30 cm behind it 110 cm along, so it missed and the far-face ray missed too ("a 90 degree outside edge", capped). The trace now also reaches what a smooth slope that began at the capsule has receded by. A face exactly at `MaxSmoothAngle` no longer flips to a corner on float noise (0.25 degree allowance).
   - **The first ticks after a grab read the gap error as a feature** (finding 2: `slope` / `bump` on the flat lane). The capsule is a few cm off its seat (entry sweep reach, rate limited pull-in) and the plane was assumed seated at `WallGap`. The last contact point (ridden with the base) now gives the plane when the capsule is more than 1.5 cm off its seat; a seated capsule keeps the seat plane.
   - **By design, not changed:** a recess deeper than `MaxStepHeight` (lane e, 15 cm) that the capsule has dived into is left over a face taller than a step, which is an inside corner (6.6); the test `RecessDeeperThanAStepIsACornerOnTheWayOut` documents it (never below the corner speed, wall kept). In the second run it reads 560 / 548 cm/s, i.e. nearly nothing. If the owner wants a deeper recess bridged, that is a decision about the adhesion (a limit on how far the capsule dives below its seat), not about classification.
2. **Climb-only wall never grabbed.** The cause was data: `setup_wall_slide_modes.py --use v2climb` copied the replaced transition's `SurfaceFilter` (RequiredBehaviours = WallSlide) into the surface-driven twin, which applied it in its sweep before the mode list was asked ("ENTRY rejected: surface filter" every tick). Not the harness stick, the surface component, the mode transition or the ability. The surface-driven transitions now ignore `SurfaceFilter` (`GetEntrySurfaceFilter()`, the list is the filter), so the shared Blueprint needed no edit; the script no longer copies the filter. Result: `j_climb_only` is grabbed by `WallClimbingV2` and climbs to the top of the wall; `j_no_slide` is refused for the list's own reason ("no registered, unblocked wall mode takes this surface", `--log 2`).
3. **Moving walls kept reconciling.** `AGSTN_WallTestMover` now poses from the simulation time of each Mover tick (`OnPreSimulationTick`, `FMoverTimeStep::BaseSimTimeMs`, server timespace), which is what a client predicting 25-48 frames ahead and a re-simulation both need; the server's clock alone (`GetServerWorldTimeSeconds`) would not do, and is only the fallback when no simulation ticks. The poses of a given sim time are identical on host and client (checked from the logs). That was not enough: with the walls frozen the lanes still reconciled 35-60 times. The grab of the first run came ~0.1 s after the harness's teleport and launch, inside the ~0.6 s in which those two commands, applied to the server's and to the client's copy of the pawn at frames 25 apart, make their own correction; the ridden-wall record (a rollback blackboard entry) is rewritten by that correction and the ride starts from a wrong record (server and client held different base records from the grab on, a constant ~20 cm offset that no later correction removes). The moving lanes now start 3 m from the wall, so the grab comes after ~1 s. Reconciles after the run's first 0.6 s: 1 per run on both moving lanes and both modes (was v2 4-16, legacy 37-50). ~~Open: a grab on a moving wall inside a correction window leaves a base record the next corrections do not repair (a real prediction weakness of the base riding, now only reachable by a grab during a correction).~~ Fixed for the v2 modes in 6.14 (the cause was any correction while riding, not the grab).
4. **Builder.** `START_HEIGHT` 1000, `WALL_HEIGHT` 1200, slant plates long enough for the new start, concave chains and curves built from 150 cm plates (behind a thin plate that curves toward the character the look's lifted point is in the air and reads "the wall ends"), moving walls start 3 m out. The map is regenerated by `build_wall_slide_ab_map.py` (idempotent: a second run found it up to date).

Harness: the end-of-run summary leaves the leaving tick out of the speed windows, shows which mode ran (`WallSlidingV2 (ran WallClimbingV2)`), `--log N` sets `gstn.Wall.Log` for the run, `--freeze-movers` stops the moving walls (diagnostic), and after the first 0.6 s a diagonal stick becomes a pure sideways one: the grab needs intent into the wall, but a wall that bends away turns "into the wall" into "up" (6.7) and the vertical brake held the character after every corner.

### 6.13 Step up and step down, query cost (owner, 2026-10-25)

- **Separate step limits.** `FGSTN_WallEdgeSettings` has a step up (`MaxStepHeight`, shown as "Max Step Up Height") and a step down (`MaxStepDownDepth`),
  10 cm each by default, like the walking mode's step height. `ClassifyAhead` uses the step up for a feature that stands out of the plane (negative offset)
  and the step down for one that recedes (positive offset). The rise out of a recess the capsule has dived into is a step up: to cross the 15 cm recess of
  lane e without the corner slowdown, raise the step up to 15 cm or more (test `GSTN.Wall.V2.Lane.RecessWithinTheStepUpIsNotACorner`, both at 20 cm).
  Defaults equal, so behaviour is unchanged.
- **Query cost on curves and near corners: accepted for now, revisit if needed.** The arc/crease sampling of 6.12 brings v2 to 5-5.7 queries per tick on
  average (max 9) on the curve and corner lanes, against the 3.5 target of the flat lane. The owner accepted it for now (2026-10-25); if the wall slide shows
  up in a profile, the first candidates are sampling the chain every other tick and caching it while the same corner stays in the look.

### 6.14 Corrections while riding a moving wall (6.12 item 3, 2026-10-25)

- **Cause.** The ride (`ApplyMovementBase` -> `UGSTN_WallMovementUtils::TryMoveWithWallBase`) moved the capsule to `BaseNow * LocalPosition`, where `LocalPosition` (the character in the base's frame)
  was written at the end of the previous tick into `GSTNWallBlackboard::BaseInfo`, a Mover rollback blackboard entry. On a correction Network Prediction restores the authority's sync
  state (`UMoverComponent::OnSimulationRollback` -> `SetFrameStateFromContext` puts the capsule there) and the blackboard rewinds to the frame before, but the value it rewinds to is **this machine's own prediction** of
  that frame. The first re-simulated tick then rode the capsule back to the client's recorded place on the wall: the correction was undone and every later state differed from the
  server's by the same offset. Nothing about the grab: any correction while riding did it; a grab inside the launch correction just made one certain. Evidence (A/B harness, lanes h,
  start 50 cm from the wall, `--start-gap 50`, grab ~0.1 s after the launch): v2 reconciles 36 (33 after the first 0.6 s) on `h_moving150` and 85 (64) on `h_moving300`; a mismatch
  printout of `h_moving300`: received `Loc Y=32858.90 Z=848.84`, local `Y=32877.70 Z=851.26` (the ~20 cm along the wall's travel).
- **Fix.** `UGSTN_WallMovementModeBase::bRideFromSyncedLocation` (advanced, default off: the legacy modes are unchanged; on in `UGSTN_WallTraverseModeBase`, i.e. both v2 modes).
  With it the ride takes the character's place on the base from the **start sync state's location** (what a correction writes), seen in the base's frame of the previous tick;
  only the base component and its pose at the end of the previous frame come from the record:

      prev  = Blackboard[BaseInfo]                     // base, its pose at the end of the previous frame
      start = StartState.MoverDefaultSyncState.Location   // the capsule's component location when there is none
      local = prev.BaseRotation^-1 * (start - prev.BaseLocation)
      move capsule by (BaseNow.Location + BaseNow.Rotation * local) - capsule; rotate normal, velocity, capsule by BaseNow.Rotation * prev.BaseRotation^-1

  Rollback-safe because the ride is now a function of synced state (the location) and of the base's own pose history; the pose of a frame is the same on every machine when the
  base is posed from the simulation time (`AGSTN_WallTestMover`, 6.12). The one record that can still disagree is "was there a base at the frame before": a client that grabbed a
  frame later than the server skips one frame of riding after the correction (at most one frame of wall motion, 2.5-5 cm on lanes h), which nothing stores, so the next correction
  repairs it. In steady state the start location is the location `LocalPosition` was recorded from, so the ride is the same as before up to float rounding (the normal-start lanes below are unchanged). Not done: putting the base and its pose in
  the sync state as Mover's own based movement does (`FMoverDefaultSyncState::MovementBase*`, rebased in `SetFrameStateFromContext`); only needed for a base whose pose differs between
  machines for the same frame (simulated physics bodies), a decision for when such walls exist.
- **Test.** `GSTN.Wall.V2Net.CorrectionOnMovingWallConverges`: a "server" rides a translating and turning wall from a start 20 cm away from the "client"'s; at frame 20 the client gets the
  server's state of frame 19 (blackboard `BeginRollback`, capsule and synced fields restored, wall back to its frame-19 pose) and re-simulates. Ride from the synced location: re-simulated
  frames and the final base record equal the server's (0.000 cm); legacy ride (the control): 20.22 cm for good, the record keeps the client's own place. Automation 885/885.
- **Harness, after** (listen host + 1 client, 60/15 ms, 0 % loss; reconciles all / after 0.6 s). Early grab (`--start-gap 50`): v2 `h_moving150` 3 / 0, `h_moving300` 5 / 0 (were 36 / 33
  and 85 / 64); the legacy slide, untouched, still 124 / 101 on `h_moving150` (1 / 0 on `h_moving300`). Normal start (3 m, the map as built): 1 / 0 on both lanes and both modes, as in 6.12.
  Standard wall scenarios (`DefaultLevel_Harness`, listen host + 2 clients, `WALL_MODE = 'WallSlidingV2'`, entry transitions pointed at it in memory): jump at the wall, server dash and
  buff on the wall, `force_reconcile_on_mode` (reconciled), at 0 % and 2 % loss: `report()` CMP 6/6 before and after both; the static wall has no dynamic base, so the change is a no-op there.
  The 3 m start of the moving lanes is kept (the map is not rebuilt); `wall_ab.py --start-gap N` moves the start of lanes h for a run.
