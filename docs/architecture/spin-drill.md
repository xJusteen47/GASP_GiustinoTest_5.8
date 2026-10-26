# Spin and drill (`GSTNSpinDrill` plugin) — specification

Status: **draft for owner review** (2026-10-25). Design only, no code. Owner questions are in §14 (Italian) and block the
checkpoints marked in the plan ([plans/spin-drill-plan.md](plans/spin-drill-plan.md)). Where a question is open the text says
*Proposal* and the plan's assumptions table states what the code would do if the owner does not answer.

`game-design-direction.md` wins over this document. `modularity-and-tags.md` §3.7–3.10, §4 and §5b are binding.

## 1. Scope and owner direction

Owner direction (project thread, 2026-10-25 18:57 and 18:59):

1. **Spin like Super Mario 3D World on the ground**: a short spin attack all around the character; locomotion continues.
2. **Spin in the air** (Sunshine feel): an air spin gives a **small hop** and **lowers gravity**.
3. A spin can be **cancelled early by spinning in the opposite direction**.
4. **Drill**: during a spin, **holding crouch** enters drill mode; **releasing crouch stops it**.
5. When the drill reaches **something destructible** (level elements, enemies) it **latches for a while** and hits it **many times** to
   destroy it. Like a drill bit, the character first does a **radial wobble**, then **stabilises** once seated.
6. After drilling for a while: a **final jump**. If the drill **hits plain ground** it only goes into **ending lag**.
7. It should be **split into several gameplay abilities**; a montage alone is not enough.

Out of scope here: the wall-slide animation set from the same message (another thread), final art (owner rule 2026-10-24: logic
first, aesthetics later), AI use of the spin (enemies may bind the same abilities later; nothing here prevents it).

## 2. Reference behaviour (what "feels right" means, measurable)

| Moment | Reference | Target numbers (designer data, defaults) |
|---|---|---|
| Ground spin | 3D World spin: ~0.5 s, hitbox ring around the body, the character keeps running, can chain into a jump | 30 frames total, actionable at 18, hitbox ring frames 2–16, radius 110 cm, speed ×1.0 (no movement lock) |
| Air spin | Sunshine/3D World hover: a little upward pop, slower fall while spinning, **once per airtime** (refreshed by landing, wall contact, trampoline, final jump) | hop +350 cm/s along gravity-up, `GravityScale` 0.35 for the spin, `MaxFallSpeed` ×0.4, 1 use per airtime |
| Opposite-spin cancel | rotate the stick the other way during a spin: the spin stops at once | cancel window frames 4–(end), recovery 4 frames, no hitbox after the cancel frame |
| Drill entry | hold crouch during a spin (ground or air): tuck pose, plunge along gravity | entry from spin frame ≥ 3 while crouch is held; plunge speed 2200 cm/s, steering 15 % |
| Drill on a destructible | latch, wobble, seat, multi-hit, then final jump | seat 12 frames (wobble decays), hit every 6 frames, max latch 120 frames, final jump after ≥ 45 latched frames |
| Drill on plain ground | thud, ending lag | 24 frames landing lag, no bounce |

Numbers are starting points for the playtest; every one is a property on a data asset (§6).

## 3. Ability split (owner: "several abilities, a montage is not enough")

Every piece is a pluggable gameplay ability bound by the kit (modularity §3.10). Each ability has its own timeline, so frame data,
buffering, cancel windows, hitboxes and hitstop come from the combat core instead of ad-hoc timers.

| Ability (class, plugin `GSTNSpinDrill`) | Base | Command (bindings) | Role |
|---|---|---|---|
| `UGSTN_GA_Spin` | `UGSTN_GA_BaseCombat` | `Command.Movement.Spin.Clockwise`, `.CounterClockwise` (+ optional `Command.Movement.Spin.Button`) — two bindings: grounded timeline, airborne timeline | Spin attack. Records its rotation sense in the drill fragment. Air binding adds the hop and the low-gravity window and consumes the per-airtime use |
| `UGSTN_GA_SpinCancel` | `UGSTN_GA_BaseCombat` | the **opposite** spin command, reachable only through the spin's cancel window (§7.3) | Ends the spin early: a tiny recovery timeline, no hitbox, keeps momentum |
| `UGSTN_GA_DrillDive` | `UGSTN_GA_BaseCombat` | `Command.Movement.Drill` = `Hold(Crouch)` while `Combat.State.Spinning` (cancel window of the spin into it) | Tuck + plunge along gravity, drill hitbox at the tip. Outcomes: latch (→ Bore), plain ground (→ its own landing lag), non-drillable hittable (→ bounce, §7.6), release (→ ends, falls) |
| `UGSTN_GA_DrillBore` | `UGSTN_GA_BaseCombat` with a **loop** section | adopted at latch (§7.4), never bound to a button | Latched phase: seat (wobble), multi-hit loop while crouch is held, ends on release / max time / target destroyed |
| `UGSTN_GA_DrillLaunch` | `UGSTN_GA_BaseCombat` | adopted at the end of Bore when the jump condition holds | Final jump away from the target; refreshes the air spin |

Why five and not one: each piece has its own frame data and montage, designers can swap any of them per kit (e.g. a heavy kit
whose Bore never launches, a kit without air spin), and the core already handles chaining abilities (cancel windows for player
input, adoption for automatic hand-offs, combat.md §11.2). The drill-specific state that must survive across abilities (target,
anchor, latch frame, spin sense, air-spin use) lives in **one sync fragment** owned by the plugin (§7.1), not in the abilities.

Phase diagram (one entity):

```
            Spin (ground|air) ──opposite spin──▶ SpinCancel ──▶ idle
                 │ hold Crouch (cancel window)
                 ▼
             DrillDive ──release──▶ idle (falls, air spin already used)
          ┌──────┼───────────────┬──────────────────────┐
   tip hits      │ tip hits       │ lands on plain       │ tip hits a hittable
   drillable     │                │ ground               │ that is not drillable
          ▼                       ▼                      ▼
     DrillBore               landing lag (Dive)     one hit + bounce (Dive ends)
   seat → loop while held
   ├─ release before MinBoreFramesForLaunch ─▶ detach pop ─▶ idle
   ├─ release after it / MaxBoreFrames / target destroyed ─▶ DrillLaunch ─▶ idle (air spin refreshed)
   └─ driller hit and staggered / target vanished ─▶ detach (hitstun wins)
```

## 4. Plugin and module boundaries (*Proposal*)

New plugin **`GSTNSpinDrill`** (one feature, enable/disable as a unit; modularity §3.3 shape):

| Module | Type | Depends on | Holds |
|---|---|---|---|
| `GSTNSpinDrillCore` | Runtime, no UObject logic beyond data | `GSTNCombatCore`, `GSTNInputCore` | Data structs (§6), pure math (`UGSTN_DrillMath`: anchor quantisation, wobble curve evaluation, launch vector), drill target response struct, native tags (§11), unit tests `GSTNSpinDrill.Core.*` |
| `GSTNSpinDrillNP` | Runtime | Core, `GSTNCombatNP`, Mover, NetworkPrediction | `FGSTN_DrillFragment` (sync fragment), `FGSTN_DrillTickExtension` (combat tick hooks), `FGSTN_DrillSnapshotExtension` (target response in the step snapshot), `UGSTN_DrillLatchedMovementMode` + modifier/transition, latch anchor provider for the attacker root (seam S3) |
| `GSTNSpinDrillGAS` | Runtime | Core, NP, `GSTNCombatGAS` | The five abilities, `UGSTN_SpinDrillTuning` data asset, kit extension `FGSTN_SpinDrillKitData`, BP hooks (`BlueprintNativeEvent`, pure) |
| `GSTNSpinDrillPresentation` | Runtime (client and listen host; dedicated server skips) | Core, NP | `UGSTN_DrillPresentationComponent` (procedural spin + wobble on the mesh, §9) |
| `GSTNSpinDrillTests` | Runtime test module | all above | Harness abilities/content names (never in shipping) |

Optional bridges (separate small plugins, created only when needed):

- **`GSTNSpinDrillParts`** (`GSTNSpinDrill` + `GSTNParts`): drill on a specific part: latch picks the part under the tip, multi-hit
  goes into that part (penetration vs hardness as M4b §3.4), a broken part ends the bore (or continues to the next part, data).
  Without it the drill works on whole entities through the generic hit pipeline.
- **`GSTNSpinDrillLevelObjects`** (`GSTNSpinDrill` + `GSTNLevelObjects`): `AGSTN_DrillableProp` (blocks, crates, walls) as a hittable
  entity with a hurtbox set and hit points, destroyed by the bore. *Open: what exists in levels (§14 SD.9).*

`GSTNSpinDrill` names neither `GSTNParts` nor `GSTNLevelObjects`. `GSTNCombat` names no drill type. Plugin docs: `README.md` with
purpose, modules, data, "how to add a drillable thing", "how to make a kit spin".

## 5. Input

- **Spin command**: the standard input grammar already has a `Spin` condition with a rotation sense (`EGSTN_SpinRotation`
  `Any/Clockwise/CounterClockwise`, `GSTN_InputConditions.cpp`, tested both senses). Two slots, one per sense, in the `Stick` space
  (combat.md §9.1: spin inputs do not depend on facing). *Proposal*: also an optional button slot (`Input.Button.Spin`, leaf declared
  by `GSTNSpinDrillCore`) whose sense is the last stick rotation sense or clockwise by default, for players who prefer a button
  (3D World style). Which one the default kit uses: §14 SD.1.
- **Opposite-spin cancel**: during a spin, the slot of the **other** sense is the only way into `UGSTN_GA_SpinCancel` (§7.3).
  Reading of the owner's line "si dovrebbe annullare prima spinando nella direzione opposta": the opposite rotation stops the
  spin early; it does not start a new spin. Confirm: §14 SD.2.
- **Drill**: `Hold(Crouch)` read from the history (combat.md §9.1: slots read the held state, not Mover's crouch tag). Release is
  read by the bore every frame through `IsTriggerButtonHeld()` / `bEndOnTriggerRelease` semantics on the crouch button (the
  trigger of `DrillDive` is crouch, so Dive and Bore both see its held state; Bore receives it through the activation context
  copied at adoption, §7.4).
- Fast fall also proposes crouch-in-air (combat.md §11b, *Open*): the drill wins only while a spin is active (its command requires
  `Combat.State.Spinning`), so the two do not conflict. Noted for the fast-fall decision.

## 6. Data (designer-editable)

All tuning is data; nothing below is a C++ constant. Content is soft-referenced (modularity §3.9).

- **`UGSTN_SpinDrillTuning`** (`UPrimaryDataAsset`, `GSTNSpinDrillGAS`), referenced by the kit extension. Groups:
  - *Air spin*: `HopSpeed`, `SpinGravityScale`, `SpinMaxFallSpeedScale`, `LowGravityExtraFrames` (keep low gravity after the spin
    timeline ends, 0 = only during the spin; §14 SD.3), `UsesPerAirtime` (1), `RefreshOn` (flags: Land, Wall, Trampoline, Launch, Hit).
  - *Cancel*: `CancelFromFrame`, `CancelRecoveryFrames`.
  - *Dive*: `DiveDirection` (`Gravity` default; `Facing`, `Stick` for later, §14 SD.6), `PlungeSpeed`, `SteerFraction`,
    `MinDiveFramesBeforeLatch` (avoid latching on the frame you start), `GroundLagFrames`, `BounceSpeed`.
  - *Bore*: `SeatFrames`, `RehitIntervalFrames`, `MaxBoreFrames`, `MinBoreFramesForLaunch`, `DetachPopSpeed`, `LatchDepthCm`
    (how deep the tip sits in the target), `bContinueThroughDestroyed` (Sunshine-style keep plunging after the target breaks,
    §14 SD.7).
  - *Launch*: `LaunchSpeed`, `LaunchDirection` (away from the target surface normal or gravity-up), `bRefreshAirSpin`.
  - *Wobble (presentation, §9)*: `WobbleCurve` (amplitude over seat time), `PrecessionHz`, `VisualSpinRps`.
- **Per-ability timelines**: each ability carries its own `FGSTN_CombatTimelineData` (inline or shared asset) with montage,
  hitboxes, cancel windows, movement track, landing lag. The tuning asset holds numbers the code needs between abilities; frame
  data of a single ability stays in its timeline.
- **Target side: `FGSTN_DrillTargetResponse`** (`GSTNSpinDrillCore`): `bDrillable`, `SeatFramesScale`, `MaxBoreFramesOverride`,
  `DamagePerHitScale`, `bLatchMoves` (the target may move while latched, e.g. a walking enemy; false for static props),
  `LaunchOnDestroyed`. Supplied by:
  1. the victim's kit or definition through a `FGSTN_KitExtension` entry (fighters, enemies);
  2. a component interface `IGSTN_DrillTarget` on non-kit actors (props), read **once at registration** into the snapshot table,
     never live during simulation;
  3. default: not drillable (the drill then behaves as "hittable, not drillable", §7.6).
  The response is copied into the per-step snapshot by `FGSTN_DrillSnapshotExtension` (seam `FGSTN_SnapshotExtension`, already
  used by M4b), so the attacker side decides latching from snapshot data only.
  Note (2026-10-25 19:40): the M4b thread is adding the contributor registry for snapshot extensions (iterated by type id) and a
  fixed 20-byte POD extension memory in the lag-comp history record; the drill response must therefore stay a small POD (≤ 20 B,
  today ~8 B) and registers as one more contributor, no further core change.
- **Drill hit data**: the tip hitbox is an ordinary `FGSTN_HitboxSpec` with `HitType = Combat.HitType.Drill` (leaf of
  `GSTNSpinDrillCore`), so the resolution pipeline, parts, hardness/penetration, teams and relationship variants all apply
  unchanged. Low knockback, low hitstun by default, attacker hitstop small (a drill grinds, it does not pop).

## 7. Simulation and network

### 7.1 Sync fragment `FGSTN_DrillFragment` (`GSTNSpinDrillNP`, feature id from the central `GSTNFeatureIds`)

| Field | Bits | Meaning |
|---|---|---|
| `Phase` | 3 | None, Spinning, Diving, Seating, Boring, Launching, Lag |
| `SpinSense` | 1 | clockwise / counter-clockwise of the current spin (for the cancel) |
| `AirSpinsUsed` | 2 | per airtime; reset by the refresh events |
| `Target` | 32 | `FGSTN_EntityRef` (combat core type of M6 §7.0 item 1: kind, id, serial). *If M6 has not landed:* NP proxy id + serial in the same 4 bytes, identical layout |
| `LatchFrame` | 32 | first latched frame (seat and wobble time origin on every machine) |
| `AnchorQ` | 3×16 | tip position in the target's root space, 1 cm quantised |
| `AxisQ` | 2×16 | drill axis in target space (octahedral), so the bore follows a rotating target |
| `HitsLanded` | 8 | for data rules (e.g. launch after N hits) and debug |

Idle cost: the fragment is absent (1 bit). Active: ~20 bytes listed. Ordinary sync state: restored on rollback, re-simulated.

### 7.2 Per-frame (tick extension hook points, `IGSTN_CombatTickExtension`)

- `AfterClocks`: advance phase timers from the combat clock (hitstop pauses them); refresh air-spin uses from the landed flag /
  contact record of F-1; detect target loss (snapshot no longer has the entity or its response says not drillable).
- `BeforeCandidates`: the bore masks its tip hit group except on rehit frames if seam S1 is not available (fallback, §8).
- `AfterExecute`: hand-offs (Dive→Bore adoption request on latch, Bore→Launch / detach at end). Writes requests only.
- `BeforeTagProjection`: projects `Combat.State.Spinning`, `.Drilling`, `.DrillLatched` (leaves declared by this plugin in the open
  `Combat.State.*` subtree) so bindings, other abilities and animation can read them.

Nothing in the extension reads components, the world or the ability system (gas-npp.md rules).

### 7.3 Spin and cancel

- Ground timeline: hitbox ring (`Root` space, sphere/capsule keys around the body), no `Combat.Lock.Movement`, cancel windows into
  jump (`Always`), into the drill (frames ≥ `DiveFromFrame`, while crouch held), into the opposite spin (frames ≥ `CancelFromFrame`).
- Air timeline: same plus a `Movement` track: additive hop (`MixMode` additive, momentum kept) and `GravityScale` =
  `SpinGravityScale` for the track's frames. Fall-speed cap: the stats modifier already rewrites Mover's caps per frame
  (combat.md §11b); the drill fragment says "low gravity active until frame X" and the stats modifier reads it through a
  generic **gravity-scale contributor** (seam S6) so `LowGravityExtraFrames` can outlive the timeline.
- Air spin refused when `AirSpinsUsed ≥ UsesPerAirtime` (activation requirement through the projected
  `Combat.State.AirSpinSpent` tag).
- Cancel: the spin's cancel window names the command of the **opposite** sense; the binding of that command while
  `Combat.State.Spinning` maps to `UGSTN_GA_SpinCancel` (a binding with a requirement tag, combat.md §9.2), outside a spin the same
  command maps to a normal spin. The sense check lives in the binding requirements, not in code: the spin projects
  `Combat.State.Spinning.Clockwise` or `.CounterClockwise`.

### 7.4 Dive, latch, bore

- **Dive**: `Movement` track overrides velocity along `DiveDirection` (`Gravity` uses the entity's gravity, so it works with any
  gravity, design direction "gravity can change arbitrarily"), `bCancelOnLand` with `LandingLagFrames = GroundLagFrames` gives the
  plain-ground ending lag with existing code.
- **Latch decision** (attacker side, frame F, predicted): the tip hitbox overlaps a hurtbox whose entity's snapshot response is
  `bDrillable`. The attacker hit hook (seam S4) reports `{victim ref, contact point, contact normal}`; the extension writes
  `Phase = Seating`, `Target`, `LatchFrame = F+1`, `AnchorQ`, `AxisQ`, and requests adoption of `DrillBore` (combat.md §11.2; one
  frame, accepted). The first contact also goes to the victim's inbox as a normal hit (impact hit), resolved on the server at F+1.
- **Latched position** (same idea as M6 §7.3 parked roles): the driller enters `UGSTN_DrillLatchedMovementMode` (zero velocity, no
  gravity, no input, collision with the target ignored). For `bLatchMoves = false` (props) the capsule simply stays at the latch
  point. For moving targets the **capsule is parked** where it latched and everything that must follow the target uses the
  **derived transform** = target root at end of F-1 (snapshot) × anchor: the tip hitbox (seam S3: attacker root override from an
  extension), the mesh (presentation) and the camera target. The server decides with the authoritative target; the owning client
  predicts with its interpolated view of the target and is corrected if they disagree (the same trade as M6 grabs, budgeted).
  On exit the capsule is placed at the derived transform before the launch (exit placement as M6 §7.7; until M6 exists a small
  local sweep from the derived point to free space).
- **Seat** (`SeatFrames` × target's `SeatFramesScale`): no damage, wobble plays (§9). *Proposal*: the seat is gameplay-relevant (a
  window in which the target can shake the driller off), §14 SD.5.
- **Multi-hit loop**: the bore timeline has a loop section [`LoopStart`, `LoopEnd`) repeated while crouch is held and the target is
  valid (seam S2); the tip hitbox re-hits the same target every `RehitIntervalFrames` (seam S1). Every hit is an ordinary hit through
  the pipeline: parts, hardness, team rules, hitstop all apply. A part or entity that becomes broken/destroyed raises the existing
  events (`Combat.Event.Part.Broken`, `.AllBroken`, or the prop's destruction event); the extension sees target loss at
  `AfterClocks` of the next frame.
- **End conditions** (evaluated in order, first match): driller in hitstun or KO → detach, hitstun wins (no launch); target gone or
  no longer drillable → launch (or keep plunging if `bContinueThroughDestroyed`); crouch released before
  `MinBoreFramesForLaunch` → detach pop; crouch released after it, or `MaxBoreFrames` reached → `DrillLaunch` adopted.
- **Launch**: `FGSTN_LayeredMove_Launch` (existing, via GE) along `LaunchDirection`; refreshes `AirSpinsUsed`.

### 7.5 Prediction and listen server

- Owning client predicts spin, cancel, dive, latch, bore phase changes and launch (all are its own state). Damage to the target is
  server-authoritative as every victim reaction (combat.md §12.7). Multi-hits on an **enemy** confirmed on the attacker's client
  through hit presentation (§12.7: flash/sparks immediately, cancelled if not confirmed).
- Disagreement cases: the client latched on a target the server says it missed (interpolation offset) → correction, the client
  snaps back to diving; budgeted like a mispredicted hit confirm. Measured in the harness at 60/15 ms and 2 % loss.
- Listen server: the host's own drill is simulated with authority and rendered on the host; presentation code runs on the host
  like any client (never branches on net mode).
- Drilling **another player** (PvP): the victim is server-authoritative and receives each multi-hit as a correction (one rollback
  per hit, combat.md §12.7). *Proposal*: players are not drillable by default (`bDrillable = false` on the player kit) unless a mode
  enables it; §14 SD.8.

### 7.6 Hittable but not drillable

The tip hit lands as a normal hit (damage, small knockback), the Dive ends with `BounceSpeed` up along -dive direction (Mario
stomp feel), the air spin is **not** refreshed. *Proposal*, §14 SD.4.

## 8. Seams needed in the combat core (owned by `GSTNCombat`; additive, default does nothing)

These are generic and useful beyond the drill. All are small, but they edit `GSTNCombat*`, which the M4b thread is changing on the
laptop right now: they are scheduled **after M4b P7–P14** or agreed with that thread first (plan §3).

| Seam | Where | What | Already designed? |
|---|---|---|---|
| **S1 Re-hit interval** | `FGSTN_HitboxSpec` (`GSTNCombatCore`) + attacker hit ring | `RehitIntervalFrames` (0 = once per group per target, today's behaviour): the same target can be hit again after N timeline frames | Yes, combat.md §11.1 ("a hitbox with a re-hit interval"); not built |
| **S2 Loop section** | `FGSTN_TimelineTrack_Loop` (`GSTNCombatCore`) + timeline player | Repeat [`Start`, `End`) while a condition holds (trigger held, tag present) up to `MaxLoops`; synced montage loops its section | Yes, the `Charge` track of combat.md §11.3 ("loop while held"); not built |
| **S3 Attacker root override** | `FGSTN_AttackerTickInput` (`GSTNCombatNP`) | An extension may replace the attacker's root transform used for `Root`-space hitboxes this frame (the derived latch transform) | New; the same need exists in M6 for parked grabbers/riders (anchor substitution, M6 §7.0 item 7) |
| **S4 Attacker hit hook** | attacker side, after a hit is recorded | Extensions get `{victim ref, hitbox key, contact point, normal}` of the frame's hits (read-only), to write their own fragment | Partly: `IGSTN_AttackerOutcomeExtension` evaluates outcomes but returns only an enum; a notify-only hook is new |
| **S5 Feature id** | central `GSTNFeatureIds` | One id for the drill fragment | Routine |
| **S6 Gravity-scale contributor** | `UGSTN_StatsMovementModifier` (`GSTNCombatNP`) | Extensions may contribute a gravity scale / fall-speed cap for the frame (min/mul with the jump phase) | New; the timeline `Movement` track covers the in-timeline part, this only serves `LowGravityExtraFrames > 0` |

Fallbacks if a seam is refused: S1 → the bore timeline repeats N hit groups (hit group per hit, limited by the 8-bit group
mask, so at most ~16 hits per bore); S2 → the bore re-adopts itself every cycle (one frame gap per cycle, visible as a hitch in
the montage); S3 → only static targets are drillable in v1; S6 → low gravity only for the spin's own frames.

## 9. Presentation and animation (logic now, art later)

Owner rule 2026-10-24: no aesthetics work now; but the **logic** of animation must exist, because a montage alone is not enough.

- **Animation states needed** (placeholder clips allowed, final art later): ground spin, air spin, spin cancel, drill tuck (enter),
  drill plunge loop, latch impact, bore loop (seated), detach pop, final launch, ground thud + lag. Each ability's timeline owns
  its montage/section; loops use the loop section (S2).
- **Decoupled from the AnimBP, swappable per skin** (owner direction 2026-10-25 19:01): each ability carries its own animation
  set; the AnimBP only offers generic slots/layers and does not know the spin or the drill. The general pattern (ability-owned
  animation sets, skin overrides) is designed by the thread "Animare la wall slide nell'ABP"; this plugin follows it and adds no
  pattern of its own. Rule for the drill: a skin (or a skin trait) may replace any of the montages above, but **hitboxes never
  change**: hitbox keys and `BakedSocket` frames are baked from the ability's **reference** timeline montage and the simulation
  never reads the skin's montage (combat.md §12.1: no animation evaluated at runtime). The skin montage is presentation only and is
  validated (same length or a declared time remap) so the visual stays in step with the frame data.
- **Names from that pattern** (`docs/architecture/anim-layers.md`, branch `claude/project-thread-2vuw3f`; part B still needs the owner
  OK): the abilities name only their animation-set tags `Anim.Set.Spin` and `Anim.Set.Drill`, never assets. A
  `UGSTN_AnimSetDefinition` per set holds the layer class and the montage overrides (`Reference` → `Visual`); skins, weapons and kits
  pick the winner through `UGSTN_AnimSetCollection` rules (skin > weapon > kit > character), and `UGSTN_AnimSetComponent` resolves
  the visual montage at runtime. The procedural wobble and the bore loop live in a linked layer `ALI_GSTN_Drill` whose C++ parent
  implements `IGSTN_OverrideLayerSource` (part A, already built). Timelines, hitboxes and `BakedSocket` read only the `Reference`
  montage; the editor validator of that pattern checks section names, lengths (within 1 frame) and root motion. The `Anim.Set.*`
  leaves belong to that plugin's tag rules (it decides who declares them).
- **Procedural layer** `UGSTN_DrillPresentationComponent` (on every rendering machine, including the listen host; never read by the
  simulation): rotates the mesh around the drill axis at `VisualSpinRps` and applies the **radial wobble**: the axis precesses
  around the seated axis with angle `θ(t) = WobbleCurve(t)` and phase `2π·PrecessionHz·t`, `t = (Frame − LatchFrame)·dt`, so every
  machine shows the same wobble from synced data. The wobble decays to 0 at `SeatFrames` (stabilised). Mesh attached to the derived
  latch transform (target interpolated on clients, as M6 §7.3).
- Cues: `GameplayCue.SpinDrill.*` (spin whoosh, latch, grind per hit, launch, thud). Camera: follows the derived transform while
  latched (camera.md, a camera-side change: told to the camera spec owner).
- Depth marker: the dive is a strong reason to keep the depth marker visible while plunging (no change needed; noted).

## 10. Tags (native, owned by `GSTNSpinDrillCore` unless noted)

| Tag | Kind |
|---|---|
| `Command.Movement.Spin.Clockwise`, `.CounterClockwise`, `.Button`, `Command.Movement.Drill` | command leaves (family `Command.Movement` is combat's; leaves declared here) |
| `Input.Button.Spin` | optional button leaf (any plugin on `GSTNInputCore` may declare buttons) |
| `Combat.State.Spinning`, `.Spinning.Clockwise`, `.Spinning.CounterClockwise`, `.AirSpinSpent`, `.Drilling`, `.DrillLatched` | projected, never granted |
| `Combat.HitType.Drill` | hit type leaf (open subtree) |
| `Combat.Event.Drill.Latched`, `.Detached`, `.Launched`, `.GroundLag` | events (outbox / gameplay events) |
| `Ability.Movement.Spin`, `.SpinCancel`, `Ability.Combat.Drill.Dive`, `.Bore`, `.Launch` | ability identity |
| `GameplayCue.SpinDrill.*` | ini leaves |

No string tags in code; all through `UE_DECLARE_GAMEPLAY_TAG_EXTERN`.

## 11. Tests and harness

- `GSTNSpinDrill.Core.*` (no world): anchor quantisation round-trip, octahedral axis, wobble curve deterministic, launch vector with
  arbitrary gravity, end-condition order, refresh rules.
- `GSTNSpinDrill.Runtime.*` (editor world): fragment serialisation (idle 1 bit), tick extension phase machine with a fake snapshot,
  binding requirements route the opposite spin to the cancel.
- Harness scenarios (`Tools/GASNPPTest/spin_drill_scenarios.py`, 60/15 ms with and without 2 % loss, listen server, 2 clients):
  `spin_ground_hits_ring`, `spin_air_hop_low_gravity`, `air_spin_once_per_airtime`, `spin_opposite_cancel`,
  `drill_ground_lag`, `drill_latch_static_prop_multi_hit`, `drill_latch_moving_enemy`, `drill_release_early_pop`,
  `drill_launch_after_bore`, `drill_target_destroyed`, `drill_hit_while_latched_detaches`, `drill_nondrillable_bounce`,
  `drill_tilted_gravity`. Each asserts frame numbers and positions with tolerances, and counts corrections on the owning client.

## 12. Risks

| Risk | Mitigation |
|---|---|
| Latch on a moving enemy mispredicts often (client sees an interpolated target) | Static props first (v1 acceptance), moving enemies behind `bLatchMoves`; measure corrections per latch in the harness |
| Core seams collide with M4b work on the laptop | Schedule S1–S6 after P14 or agree a window with that thread; all are additive |
| Crouch-in-air also wanted for fast fall | Drill requires `Combat.State.Spinning`; fast fall stays free otherwise |
| Long bore + hitstop per hit feels sticky | Attacker hitstop multiplier ~0.3 on drill hits by default; tune in playtest |
| Spin by stick rotation is hard on keyboard | Optional button slot (`Input.Button.Spin`) |

## 13. How to extend

- New drillable thing: give it a `FGSTN_DrillTargetResponse` (kit extension or `IGSTN_DrillTarget` component) and a hurtbox set.
- New drill variant (sideways drill, wall drill): a new Dive timeline with `DiveDirection = Facing` and its own binding.
- New end rule: a Blueprint `UGSTN_DrillEndRule` (pure `BlueprintNativeEvent`, evaluated by the bore at end frames only; the
  per-frame loop stays C++).

## 13b. Owner answers of 2026-10-25 19:22 (they win over the text above where they differ)

- **Steering in the dive**: limited left/right movement while plunging (`SteerFraction` is lateral only, on the plane normal to
  the dive direction; default 15 %).
- **Exit (SD.10 answered)**: releasing crouch **before the maximum bore time** gives the **small hop out** (`DetachPopSpeed`);
  `MinBoreFramesForLaunch` is dropped. The maximum bore time **may depend on what is drilled** (`MaxBoreFramesOverride` on the
  target response, or from the part/prop data). **Pressing or holding jump close to the exit** (a window of `JumpBoostWindowFrames`
  before the exit frame, read from the input history so it is buffered and rollback-safe) turns the exit into the **higher jump**
  (`DrillLaunch`). Reaching the maximum time without a jump input: *Proposal* the small hop (same as release).
- **Not invincible by default**: no `HurtboxModifier` on the drill timelines unless a kit adds one. While latched the driller can
  be hit; being staggered detaches (§7.4 order unchanged).
- **Players are drillable (SD.8 answered)**: the latch is in practice a **hit-grab**. Consequence: on players and fighters the latch
  should be a **link** of the M6 system (`GSTNLink`, `Body` channel, new link type `Combat.Link.Type.Drill` with roles
  `Driller`/`Drilled`, declared by this plugin), so the grab rules, immunity, break rules and the watchdog of M6 apply and the
  drilled player is server-authoritative. Until M6 exists, v1 latches only on non-player targets with the fragment of §7.1; the
  fragment layout already uses `FGSTN_EntityRef` so the move to a link keeps the data.
- **What is drillable = balance (SD.4 / §6 revised)**: no hand-set `bDrillable` flag as the main rule. The drill tip carries a
  **penetration** like any hit; a part is drillable when penetration ≥ its hardness (combat-parts §3.4, e.g. heavy armour refuses a
  weak drill: ricochet/bounce as §7.6). `FGSTN_DrillTargetResponse` keeps only the overrides (max time, seat scale, `bLatchMoves`)
  and an explicit `bNeverDrillable` for special cases.
- **Demolition force** (Helldivers 2; new general mechanic, passed to the planning thread): props and meshes may require a minimum
  demolition force to be destroyed; it usually does not apply to enemies. The drill will carry a `DemolitionForce` value on its hit
  data once that mechanic exists; the drill does not define it.
- **Keyboard spin**: likely a shortcut such as A-S-D without W (a partial rotation). The `Spin` input condition takes the rotation in
  degrees, so a 180° slot (left-down-right) covers it; the general shortcut design goes to the planning thread (SD.1 partly
  answered: stick rotation + keyboard shortcut, button still open).
- **Input-dependent hitboxes** (e.g. a vertical swing that turns diagonal while the stick is held): a general timeline feature,
  passed to the planning thread; nothing in the drill needs it.
- **Where the general items now live** (planning thread, commit e84fb7b on the PR #4 branch): demolition force in
  `combat-m4-hits.md` §7c (`FGSTN_HitData::DemolitionForce`, `UGSTN_DestructibleComponent` in `GSTNLevelObjects`, plan H14): the drill
  tip carries its own value, and below a prop's threshold the drill bounces as "not drillable" (§7.6); hitboxes that change with
  other factors in `combat-m4-hits.md` §3 (plan H15); keyboard spin in `input.md` §9 (`DigitalMinDegrees` per slot plus a
  `bDigitalDirection` bit in the input frame; A-S-D is counter-clockwise, D-S-A clockwise); the reserved link row
  `Combat.Link.Type.Drill` (roles `Driller`/`Drilled`, owner `GSTNSpinDrill`) in `combat-m6-defence-links.md`; drillability by
  penetration vs hardness referenced from `combat-parts.md` §3.4. New rule that affects the tip: world occlusion with a pass-through
  radius and hurtbox openings (`combat-m4-hits.md` §9c, plans H16, P18): a thin tip can enter cavities that have an opening, a
  thick one stays on the armour, so the tip's pass radius is part of the drill data.
- **Sequencing**: these general items go to planning first; the detailed design of the individual abilities resumes after that.

**Owner OK of 2026-10-25 19:39** ("va bene a tutte"): SD.11 approved (core seams S1–S6, scheduled after M4b P14) and SD.12
approved (after M4b; the parts that do not touch the core may run on the desktop in parallel with M5). The proposals of the other
SD questions not already answered in §13b stand as decisions: SD.2 opposite rotation stops the spin, SD.3 low gravity only during the
air spin with a parameter to extend it, SD.5 no damage during the seat, SD.6 gravity direction only in v1, SD.7 final jump when the
target breaks (option to keep plunging), SD.9 both level props and enemies, a static block first. SD.1 button slot: kept optional.

## 13c. Spin-driven interactables: screws and knobs (owner brainstorm 2026-10-26 09:52, *Proposal*)

Owner idea: **screw switches** that screw in or out depending on the **spin sense**, and **knobs/valves** that turn but never come
off (New Super Mario Bros. Wii).

They are **not** a drill-specific system: they are members of an activation zone of the generic level-activation base
([level-activation.md](level-activation.md), plugin `GSTNActivation`). This plugin adds nothing on the object side; it only
**emits triggers**:

- A screw/knob = a zone with two rules, `Activation.Trigger.Spin` clockwise → +1, anticlockwise → −1 (tags and sense encoding of
  level-activation.md §2, reused here, no tags of our own), and a `UGSTN_ActivationRotationResponse` (knob: clamp at the ends, never
  detaches) or a `UGSTN_ActivationTrackResponse` for the screw's travel along its axis. The zone has one owner (first to stand,
  standing now, or assigned) and shows the owner's colour, which answers most of the "several players" case.
- **Spin trigger**: `UGSTN_GA_Spin` posts one `Activation.Trigger.Spin` event with its sense through the combat outbox, at an
  authoritative event key on its timeline. That needs seam **D-1** (game-modes.md §2), so D-1 joins the core seams of §8 (scheduled
  with S1–S6 after M4b P14). The spin's internal sense (`Combat.State.Spinning.Clockwise/.CounterClockwise`) maps 1:1 onto the
  activation encoding.
- **Drill trigger**: a drill latched on a zone member posts `Activation.Trigger.Drill` with the sense on every multi-hit tick
  (continuous turning while held). A member is latchable when it has a target response (§6) or is marked as a drill receiver in its
  zone data.
- **Network**: as level-activation.md §5 (server-authoritative zone state, responses computed from the replicated value). The
  player's own spin and drill stay predicted.
- **Owner answers 2026-10-26 10:08**: SD.15 settled (one owner per zone, as the panels). SD.14: per object (data on each screw or
  knob: detach or stop at the end, spring back or not). SD.13 answered 10:14: **designer's choice per object** (data on the screw/knob: which of spin hits and latched drill turn it, step size or continuous rate).
- **Screws on mechanical enemies** (owner idea, 10:08): specified by the planning thread in [combat-parts.md](combat-parts.md) §3.4
  "Screws and bolts" (plan checkpoint P19): the screw is a **part** with `FGSTN_PartScrewRule`; its progress reuses the part damage
  byte and it breaks at full; the armour part above it (`HeldBy` + `HeldByRule`) breaks in the same frame whatever its hardness.
  Turning comes from **hits**, not activation triggers, so it needs no D-1. This plugin's only duty: **author `TurnSense` and
  `TurnSteps` on its hitboxes** (`FGSTN_HitData`, combat-m4-hits.md): the spin ring sets `TurnSense` from the spin sense (+1
  clockwise seen from above along the attacker's up axis, −1 counter-clockwise; maps 1:1 from `Combat.State.Spinning.*`),
  `TurnSteps = 1`; the latched drill's tip sets the same sense and turns the screw once on every S1 re-hit. Zones hosted on moving
  enemies were rejected (a second state model next to the predicted parts fragment).
- **Sense convention for level triggers**: level-activation.md §3 uses the same convention for `Activation.Trigger.Spin/.Drill`
  (`int8 Sense` on the event; rules filter with `EGSTN_TriggerSense Any/Clockwise/CounterClockwise`).
- **Tests**: harness `screw_spin_steps`, `screw_drill_continuous`, `knob_clamps_no_detach` live with the activation plugin's tests and
  use the spin abilities of this plugin.

## 14. Domande per Giustino (italiano)

Ognuna ha una mia proposta; se non rispondi, il piano usa la proposta (colonna "se non rispondi" nel piano).

- **SD.1 Input dello spin.** Lo spin si fa ruotando lo stick (come Sunshine, la condizione esiste già) o con un tasto dedicato
  (come 3D World)? *Proposta:* entrambi: rotazione dello stick e un tasto `Spin` opzionale; il kit di test usa la rotazione.
- **SD.2 Annullamento.** "Si annulla spinando nella direzione opposta": intendo che la rotazione opposta **ferma** lo spin in corso
  (non ne parte un altro). Giusto? Da quale frame in poi si può annullare (subito o dopo qualche frame)?
- **SD.3 Gravità bassa.** La gravità si abbassa solo durante lo spin in aria, o resta bassa fino all'atterraggio? *Proposta:* solo
  durante lo spin, con un parametro per prolungarla.
- **SD.4 Nemico non trapanabile.** Se il trapano colpisce qualcosa che si può colpire ma non trapanare: un colpo + rimbalzo
  (tipo pestone di Mario)? *Proposta:* sì.
- **SD.5 Fase di assestamento.** Durante l'oscillazione iniziale (prima di stabilizzarsi) il trapano fa già danno? Il bersaglio
  può scrollarselo di dosso in quella finestra? *Proposta:* niente danno durante l'assestamento, nessuna regola di scrollata per ora.
- **SD.6 Direzione del trapano.** Solo verso il basso (lungo la gravità) o anche in avanti / contro i muri? E da terra: tenere
  giù durante lo spin a terra trapana il pavimento sotto (se è distruttibile) e altrimenti fa solo ending lag? *Proposta:* v1 solo
  lungo la gravità; da terra trapana solo se sotto c'è qualcosa di distruttibile, altrimenti ending lag.
- **SD.7 Bersaglio distrutto.** Quando il bersaglio si rompe durante il trapano: salto finale, oppure si continua a scendere
  trapanando (come Sunshine che sfonda)? *Proposta:* salto finale, con opzione per continuare.
- **SD.8 Giocatori.** Si possono trapanare gli altri giocatori (PvP)? *Proposta:* no di default, attivabile per modalità.
- **SD.9 Cosa è distruttibile nei livelli.** Blocchi/casse/muri come oggetti di livello con punti vita (nuovo attore in un plugin
  ponte) oppure solo nemici e loro parti (M4b)? *Proposta:* entrambi; prima un blocco statico (più facile da predire).
- **SD.10 Rilascio anticipato.** Se lasci il tasto prima del tempo minimo: piccolo distacco senza salto finale? E il salto finale
  ricarica lo spin in aria? *Proposta:* sì e sì.
- **SD.11 Modifiche al core combat (architettura).** Servono sei piccoli agganci generici nel core (`GSTNCombat`, §8): ri-colpo a
  intervallo, sezione in loop della timeline, radice dell'attaccante sostituibile, notifica dei colpi all'estensione, id del
  frammento, contributo alla gravità. Due erano già previsti dalla spec (re-hit e "Charge"). Va bene aggiungerli, programmandoli
  **dopo** M4b P14 (per non pestare i piedi al lavoro sul portatile)? *Proposta:* sì; fino ad allora si fanno solo i pezzi senza core
  (dati, tag, matematica, spin a terra).
- **SD.12 Priorità.** Dove va nella roadmap: subito dopo M4b, o dopo M5 (editor)? *Proposta:* dopo M4b, in parallelo a M5 sul
  fisso per i pezzi che non toccano il core.
- **SD.13 Come si gira una vite.** Basta colpirla con lo spin (ogni spin = un passo), oppure bisogna starci sopra / trapanarla per
  girarla in continuo? *Proposta:* entrambe: lo spin la gira a scatti, il trapano agganciato la gira in continuo finché tieni premuto.
- **SD.14 Vite svitata del tutto.** Quando si svita fino in fondo cosa succede: si stacca e cade (diventa un oggetto o una
  piattaforma), oppure si ferma al limite? E le manopole tornano indietro da sole quando smetti (a molla)? *Proposta:* scelta per
  oggetto nei dati; di default la vite si ferma al limite e la manopola non torna indietro.
- **SD.15 Più giocatori.** Viti e manopole usano la base generica di attivazione (level-activation.md), quindi hanno un
  "proprietario" come le piattaforme rosse e blu: conta solo chi ci è salito per primo (o chi ci sta sopra ora). Ti va bene anche
  per le viti, o vuoi un "tiro alla fune" (versi opposti si annullano)? *Proposta:* proprietario come le piattaforme.

