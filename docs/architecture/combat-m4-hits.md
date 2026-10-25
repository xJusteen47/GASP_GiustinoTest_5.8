# Combat M4 — Hits (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5.3, §12 and §15 there, and the M1–M3 specifications.
Section numbers below are local to this file.

> **Revision 2026-10-18:** hits publish authoritative events (`Combat.Event.HitLanded`, `HitReceived`, `KO`) through
> the outbox of combat.md §5.6 / M2 §7b, tagged with command, move, hit type and relationship.

> **Revision 2026-10-19 (combat abilities):** "move" in this file means a **timeline ability**
> (`UGSTN_GA_BaseCombat` and its timeline, combat.md §11); type names follow M3.

> **Revision 2026-10-22 (owner direction, [game-design-direction.md](game-design-direction.md), wins over this file):**
> (1) seams that [combat-parts.md](combat-parts.md) (M4b, plugin `GSTNParts`) needs, so `GSTNCombat` works without it (§3b);
> (2) knockback weight from remaining health, stagger and juggle factors, hitstun/hitstop by what hits and where (§3);
> (3) surface impacts: wall splat, bounce, destruction, by designer data (§7b); (4) dodges are **kit-dependent**: base PvE
> ground dodge = long-jump dive with feet-only intangibility, air dodge = dive roll; no roll/spot dodge, no rolling movement
> mode (§1b); (5) the reference topology is a **listen server**, not a dedicated one (§9b); (6) SDI is M6's, M4 ships DI only (§7).

> **Revision 2026-10-22 (morning, owner direction, [game-design-direction.md](game-design-direction.md) §2b; it wins over this
> file):** (1) hitstun is **conditional**: the hit's **stagger force** (derived from its damage and knockback, or set by the
> designer) must reach the **threshold of the hurtbox that was hit** (hurtbox, then entity, then rules), otherwise the hit does damage
> and hitstop only (§3); (2) hitstop is **automatic** from damage and knockback, or **set per attack** (§3); (3) **weight decides
> the reaction tier**: launch, push back or stagger in place (§3, §7). `StaggerPower`, `MinStagger` and `MaxStagger` are replaced by
> this model. All new choices are *Proposal*.

> **Revision 2026-10-22 (late morning, owner direction, game-design-direction.md §2c and §2d):** (1) **DI** changes the
> launch's yaw (lateral stick) **and elevation** (stick along the launch: DI in steeper, DI out towards horizontal, never across
> it), formula in §3; (2) **SDI is a held direction** (M6 §5.5); (3) **no dodging in hitstun or hitstop** by default (§1b);
> (4) **gravity can change arbitrarily**: every "up", "horizontal", "movement plane", launch elevation and push-back direction in
> this file is relative to the **victim's gravity at that frame** (`Up = −GravityDir`, read from the Mover sim state through the
> tracker, M2), never world Z; (5) **trampolines** exist as level objects (§7b).

> **Revision 2026-10-22 (10:30, owner answers, game-design-direction.md §2g):** (1) the base ground dodge's feet are intangible
> **only in the initial part** of the dodge (owner's correction at 11:41; the 10:34 answer said "final part") (§1b); (2) which hurtbox portions are invulnerable, when and how is **designer
> data in general**, never code (§1b); (3) gameplay logic must be **extendable in Blueprint** and content **replaceable**
> (modularity-and-tags.md §3.8, §3.9): the seams of §3b have Blueprint forms.

> **Revision 2026-10-22 (11:18, owner, game-design-direction.md §2h):** whether a hit **launches** depends on its knockback and the
> victim's **current weight**, which drops as damage accumulates (Smash-like). This is **standard logic** that generally does not
> change: `HealthWeight` is part of the standard pipeline and the standard rules asset sets real tier thresholds (§3, step 4).
> Owner's example: hit 1 staggers without launching and does damage; hit 2's knockback, against the lighter weight after hit 1,
> is still below the launch threshold, so the enemy is only staggered again. **Three dials, one job each** (owner, 11:20):
> **hardness** (the Helldivers armour value, against the attack's penetration) decides whether damage applies; **stagger
> resistance** decides whether the victim enters hitstun (hitstop can apply either way); **weight** decides the knockback's
> strength, and the reaction tier follows from it. Damage makes knockback stronger **only through the weight**, so the standard
> model is the new `WeightDriven` one (§3) and damage is never counted twice.

## 1. Scope

**Goal:** moves hit. Hitboxes from move tracks overlap hurtboxes; the attacker predicts its own hitstop and
hit-confirm (cancel windows OnHit/OnBlock become live); the victim, on the machine that simulates it, resolves the hit
through the pipeline: damage, meter, knockback with DI, hitstun, tumble, hitstop with a real freeze (also on walls).
Teams and friendly-fire variants. Lag compensation for remote attackers. Hit presentation on interpolated victims.
Hitstun that depends on the hit's **stagger force** against the threshold of the hurtbox hit, hitstop that is automatic or set by
the designer, a **reaction tier** (stagger in place, push back, launch) chosen by weight, knockback that depends on remaining health
and on the juggle factor, and the impact of a launched entity on a surface (§7b).

Also in: the **base dodges** (§1b), now that intangibility exists, and the **generic seams** of §3b.

Out: shield, parry, links and grabs, tech, SDI (all M6: DI is the only victim influence in M4, §7); piggyback (M7); minions
(M8); editor gizmos (M5); destructible body parts, penetration and hardness (M4b, `GSTNParts`: M4 only provides the seams of
§3b); a rolling movement mode (owner: not now).

## 1b. Dodges (kit-dependent)

Owner direction: there is **no standard dodge**; it depends on the kit. M4 therefore ships no dodge system, only the two
**base PvE dodges** as ordinary timeline abilities in the default kit (data, `GSTN_Kit_*`), built on mechanisms that already
exist: a movement track (M3 `FGSTN_LayeredMove_MoveTrack`) and a `HurtboxModifier` window.

| Dodge | Behaviour (base PvE kit) | Intangibility |
|---|---|---|
| Ground dodge | Long jump / dive: a committed forward leap with an arc, **ends standing** (a short landing recovery, no lying-down get-up state) | **Only the feet hurtboxes** (`Combat.Hurtbox.Feet`), and **only in the initial part** of the dodge (owner, 2026-10-22, 11:41, correcting the 10:34 "final part"): the window opens on the dodge's first active frame (take-off) and closes partway through the leap (*Proposal* default: the first 40% of the airborne frames). The rest of the body, and the feet after the window, can be hit. State `Intangible` by default (*Reading*: the owner says "invincibili"; intangible means a hit on the feet does not even register); the base kit's designer may switch the window to `Invincible` |
| Air dodge | Dive roll, as Super Mario 3D World: a forward air burst with a tuck animation, then normal falling | Not specified by the owner: *Proposal*: same rule as the ground dodge (feet only) until the owner says otherwise |

- The "roll" is an **animation and ability**, queued as a normal move through Mover (rule 2 of CLAUDE.md); **no rolling
  movement mode is implemented** (owner, game-design-direction.md §6/§8). Spot dodge and a generic roll are removed from M4.
- The `HurtboxModifier` track selects hurtboxes by name (`AffectedHurtboxes`) **or by tag** (`AffectedTags`, new, so a kit's
  dodge does not depend on another set's names); the foot group is `Combat.Hurtbox.Feet` in the hurtbox set.
- **Invulnerability is designer data, everywhere** (owner, 2026-10-22: "le porzioni delle hurtbox invincibili e quant'altro
  dovrebbero poter essere lasciate al designer, in generale"). Which hurtboxes (by name or tag), which portion of them
  (`ShapeOverrides`), which frames and which state (`Intangible`, `Invincible`, `Armor`, super armor) are `HurtboxModifier`
  track data on the move's timeline, authored in the M5 timeline editor. The base dodges' windows above are defaults of the base
  kit's data, not code: `GSTNCombat` never hard-codes a hurtbox group, a window or a state for any move, and a designer can
  reshape the base dodges without a build.
- **Dodges, jumps and the like are pluggable gameplay abilities** (owner, 2026-10-22, 11:41, "come avevamo già concordato"): each is
  an ability granted by the kit or an ability set (M3 kit bindings, modularity-and-tags.md §3.4 pawn data), never a code path in
  `GSTNCombat`, so changing the dodge later means editing or swapping that ability asset, not the framework.
- Other kits (heavy: armoured step, caster: blink) are only content: another timeline ability with another `HurtboxModifier`.
  Nothing in `GSTNCombat` assumes a particular dodge. M6's tech roll is a separate thing (tech ability, M6 §6).
- **No dodging in hitstun or hitstop** (owner, 2026-10-22: "in genere no"). The base dodges list `Combat.State.Hitstun`
  and `Combat.State.Hitstop` in their activation-blocked tags, so a dodge press during them is buffered or dropped by
  the normal buffer rules, never executed. Because the owner said "generally", it is ability data, not a hard rule in
  `GSTNCombat`: a kit can ship an escape option (a burst, a meter-paid break-out) that omits these tags.


## 2. Spikes first (record results in combat.md §17)

1. **Cross-entity cost**: client 1's pawn hits client 2's pawn (server-resolved); measure client 2's rollbacks per hit
   and the visible delay on client 1 (presentation off). Expect about one rollback per hit on client 2.
2. **Freeze**: `FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity) in Walking, Falling and each project wall mode,
   with forced reconciles inside the freeze. Confirms how a layered move exposes a Mover gameplay tag (check
   `UMoverComponent::HasGameplayTag` sources: modes, layered moves, modifiers); if layered moves cannot carry tags, the
   freeze also queues a `FGSTN_FrozenModifier` that does.
3. **Hitstop with a montage playing**: the M0 pause path with a real montage, forced reconcile in the window.
4. **Interpolation buffer**: lower `FixedTickInterpolationBufferedMS` (100 ms today) step by step under 60/15 ms and
   2 % loss and keep the smallest value without visible stutter of interpolated proxies (spike S-FP showed this buffer
   is most of the delay of remote actions, combat.md §12.7). Record it in combat.md §17.

## 3. Core types (`GSTNCombatCore`)

**Shapes** — `FGSTN_HitShape { EGSTN_ShapeType Type (Sphere/Capsule/Box); FVector Extent (radius / radius +
half-height / half-extents); FTransform Local; }`. `UGSTN_HitShapeUtils` (stateless): `Overlap(A, AWorld, B, BWorld)`
for every pair (sphere–sphere, sphere–capsule, capsule–capsule, sphere–box, capsule–box, box–box via SAT); and
`SweptOverlap` that tests `N` sub-steps between a hitbox's previous and current world transform (`bSweepBetweenFrames`
on the hitbox, N = ceil(distance / smallest extent), capped at 4) so fast hitboxes do not tunnel. All math in doubles,
results identical on every machine for identical inputs.

**Hit data** — `FGSTN_HitData { float Damage; float BaseKnockback; float KnockbackGrowth; float FixedKnockback (≥ 0
overrides); float AngleDeg; EGSTN_SpecialAngle SpecialAngle (None / Sakurai3D / AutoLink); EGSTN_YawSource YawSource
(AttackerFacing / HitboxToVictim / HitboxVelocity); float HitstunMultiplier; EGSTN_HitstopMode HitstopMode (Auto /
Override); float HitstopMultiplier; float AttackerHitstopMultiplier; int32 HitstopFramesOverride; int32
AttackerHitstopFramesOverride (−1 = same as HitstopFramesOverride); float ShieldDamage; int32 Priority; FGameplayTag HitType (Combat.HitType.Normal/Grab/
Projectile/Unblockable); TArray<TSubclassOf<UGameplayEffect>> ExtraEffects (applied to victim); FGameplayTag CueTag;
FInstancedStruct CustomData; EGSTN_StaggerForceMode StaggerForceMode (Derived / Override); float StaggerForce (the override
value); float StaggerForceScale = 1; FGSTN_ImpactProfile Impact (§7b); int32 DemolitionForce = 0 (§7c, added 2026-10-25);
TArray<FInstancedStruct> ExtensionData (base struct FGSTN_HitExtension, modularity-and-tags.md §5b; empty by default); }`.
The **stagger force** is the "what hits" input of the hitstun condition and the hitstop fields feed the model's hitstop rule
(both in the knockback model below).

**Hitbox** — `FGSTN_HitboxTrack : FGSTN_TimelineTrack { TArray<FGSTN_HitboxSpec> Hitboxes; }`, `FGSTN_HitboxSpec {
FGSTN_HitShape Shape; EGSTN_HitboxSpace Space (Root/BakedSocket/WorldAtMoveStart/Projectile); FName Socket;
TArray<FGSTN_HitboxKey> Keys (frame, offset, rotation, scale; linear interpolation); uint8 HitGroup; bool
bSweepBetweenFrames; FGSTN_HitData Data; TMap<FGameplayTag, FGSTN_HitVariant> RelationshipVariants; EGSTN_ReachAxis ReachAxis
(None/X/Y/Z in the hitbox's space, default None; added 2026-10-23 for kit mods that lengthen a weapon, combat-loadout-mods.md §8:
a mod's `ReachScale` stretches and moves only hitboxes that declare an axis); bool bScaleExtentWithReach = true;
EGSTN_HitboxZone Zone (Normal/Sweet/Sour, label only); }`.
**Weapon length, swing and stab, sweet and sour spots (owner, 2026-10-23 13:48).** Hitboxes scale with a moddable weapon's length for
**both swings and stabs**: a swing hitbox sits in `BakedSocket` space on the weapon's grip socket with `ReachAxis` along the blade; a stab
hitbox sits in `Root` (or grip) space with `ReachAxis` along the thrust. Scaling is from the space's origin: position along the axis
× `ReachScale`, extent along the axis × `ReachScale` unless `bScaleExtentWithReach` is false (a tip sweetspot that moves to the new tip
but keeps its size). **Sweetspots and sourspots** are several hitboxes of the **same `HitGroup`** with different `FGSTN_HitData` (a
strong tip, a weak base): when more than one of them overlaps the same victim in the same frame, the highest `Data.Priority` wins, ties
by the lower array index, and the group then counts as hit (step 5 of §6). `Zone` changes no rule: it drives editor colours and lets cues
pick a sweetspot sound or effect.
**Hitboxes that change with other factors (owner, 2026-10-25 19:22).** Example: a vertical sword swing that turns diagonal to the
right or to the left the longer the stick is held that way. *Proposal* (checkpoint H15, after H13):
Revised 2026-10-25 19:39 (owner: attacks whose **animation** changes with input over time, such as an aerial that rotates, and
attack variations): the value is not a hitbox field but a **timeline track**, so hitboxes, animation and facing read the same number.
- **`FGSTN_TimelineTrack_Steer`** (new track, `GSTNCombatCore`) { `EGSTN_SteerDriver Driver` (None, StickLateralHeld, StickAngle,
  HeldFrames, Extension); `int16 RatePerFrame`; `bool bReturnWhenReleased`; `int32 LatchFrame = -1`; `FName AnimBlendParameter`;
  `float FacingYawRateDegPerFrame = 0`; `TArray<FGSTN_SteerBranch> Branches` }. At most two steer tracks per timeline (values `V0`, `V1`).
- The value `V` is an integer in −1000..1000 stored in the ability's **timeline sync state**, so a rollback replays it exactly and
  simulated proxies receive it with the rest of the sync state. Each frame it moves by `RatePerFrame` towards the driver's sign, read
  from the **input history** (stick lateral axis in the attacker's facing space, combat.md §9.1, the same history the slots read), never
  from Mover queries or the camera. With `LatchFrame` ≥ 0 the value freezes at that frame (`LatchFrame = 0` gives Smash-style angled
  attacks chosen at the start); `bReturnWhenReleased` drifts it back to 0.
- **Hitboxes**: each `FGSTN_HitboxSpec` gains `TArray<FGSTN_HitboxKey> KeysAtMin, KeysAtMax` (same frames as `Keys`; empty = not
  affected) and `uint8 SteerIndex`. `V < 0` lerps offsets and slerps rotations from `Keys` towards `KeysAtMin`, `V > 0` towards
  `KeysAtMax`. Shape, `HitGroup`, `Data` and the reach rules above stay the hitbox's own.
- **Animation**: presentation reads `V` and feeds `AnimBlendParameter` (a 1D blend of, for example, the vertical and the two diagonal
  swings, or an aim offset for an angled attack), so what is seen and what hits agree on every machine.
- **Facing rotation** (the rotating aerial): with `FacingYawRateDegPerFrame` > 0 the attacker's yaw turns by `V / 1000 × rate` each
  frame through the timeline's `Movement` track (one queued layered move, combat.md §11: abilities never set the rotation directly).
  `Root`-space hitboxes and the montage turn with the character, so nothing else needs to know. A turn-around (flip facing in a
  window when the stick is held back) is the same track with a latched `StickAngle` driver and a 180° snap, as `AimPolicy` already
  allows at the start of a move.
- **Variations**: `FGSTN_SteerBranch { int32 Frame; int16 MinValue, MaxValue; FName Section; }` jumps the timeline to another
  section when `V` is in range at `Frame` (a held direction picks a different finisher). It uses the loop and section mechanism that
  spin-drill.md asks the core for (seam S2). Discrete follow-ups on a new press stay cancel windows, as today.
- M5 previews `V` with a slider (hitboxes, montage blend and facing together) and warns when a hitbox has `KeysAtMin`/`KeysAtMax`
  but its steer track names no blend parameter.
- `Extension` is a C++ driver (`FGSTN_SteerDriver`, instanced struct; per-frame code stays C++, modularity §3.8) for other factors:
  charge time, an attribute, a tag. Questions HV.1–HV.3.

**Turning and moving during a move, per move (owner, 2026-10-25 19:56).** Some moves let the character **turn a few degrees** during
startup, some let it **walk** during startup; it changes per move. In the air the character can usually **move but not turn**, apart
from these small corrections. Today the timeline has `AimPolicy` and `MaxTurnDegrees` (the turn at the start; `TurnUntilFrame` only for
the camera policy) and `FGSTN_TimelineTrack_Movement::bLockFacing`; nothing lets stick input turn or move the character inside a window.
*Proposal* (part of H15):
- **`FGSTN_TimelineTrack_Control`** (frame window, several per timeline) { `float MaxTurnDegrees` (total, relative to the facing at the
  window start); `float TurnRateDegPerFrame`; `float MoveInputScale` (0 = no locomotion from the stick, 1 = full walk on the ground or
  full air drift in the air); `bool bTurnFromMovement = false` (moving does not rotate the character) }.
- The stick is read from the input history in the attacker's facing space, as the steer track. Turn and movement go through the
  timeline's movement path (one queued layered move, combat.md §11), never by setting the rotation or the velocity directly.
- **Defaults** (*Proposal*): ground moves without a control track keep today's behaviour (no turn, no walk). Aerials get the air
  drift of the jump with turning locked (`MoveInputScale = 1`, `bTurnFromMovement = false`), so they match the owner's rule.
- **The owner's example**, a vertical swing in the air:

| Frames | Track | Effect |
|---|---|---|
| 0 | `AimPolicy = SnapToStick` | the first input decides the direction |
| 1–6 (first part of startup) | Control: `MaxTurnDegrees` 20, `TurnRateDegPerFrame` 4 | small correction of the angle |
| whole move | Control: `MoveInputScale` 1, no turning from movement | air drift without turning |
| active frames | Steer: `StickLateralHeld`, latched at the last active frame | the swing goes diagonal right or left (blend of hitboxes and animation) |

**Hitbox interpolation between frames (owner question, 2026-10-25 19:39; platform fighters).** Status in the M4 code on
`feature/gas-npp-5.8`: keys interpolate linearly between key frames (`UGSTN_HitShapeUtils::EvaluateKeys`), and `bSweepBetweenFrames`
tests sub-steps between the previous and the current frame along the arc (rotation slerped, so a swing's middle is covered, test
`SweptCatchesFastBox`), with the step count from the distance over the shape's smallest extent, capped at `MaxSweepSubsteps = 4`.
Four gaps, *Proposal* (checkpoint H17, after H13):
1. **The character's own movement is not swept**: the previous carrier keeps the current root ("the root of the previous frame is not
   state", attacker tick). A fast dash attack or a fast-falling aerial can pass through a victim. Fix: while a timeline with sweeping
   hitboxes is active, the combat attacker sync state keeps the previous root (`PrevRootLocation` quantised to 1/10 cm and
   `PrevRootYaw` as 16 bits, about 8 bytes, dirty only while sweeping) and the sweep runs on the whole carrier, root included.
2. **The step cap**: past 4 steps a very fast hitbox still skips. A sphere moving in a straight line is exactly a capsule, so sphere
   hitboxes use that exact test (as platform fighters stretch hitboxes between frames); other shapes keep sub-steps, with the cap
   as a setting and a stats counter for clamped sweeps.
3. **The first active frame**: today it sweeps from where the baked socket was one frame earlier, before the hitbox existed. Platform
   fighters do not interpolate on a hitbox's first frame: `bSweepFirstFrame = false` by default.
4. **Off by default**: `bSweepBetweenFrames` stays false in data for compatibility; the standard set turns it on, and the M5
   validator warns when a hitbox moves more than its smallest extent in one frame with the sweep off (question INTP.1).
Hurtboxes are not swept (a fast victim through a still hitbox), as in platform fighters; accepted.
`FGSTN_HitVariant { bool bIgnore; float ShapeScale = 1; FGSTN_HitDataOverride Override (per-field optional values and
multipliers); }`. `BakedSocket` uses `FGSTN_CombatTimelineData::BakedSockets (TMap<FName, TArray<FTransform>>)` — one
root-relative transform per frame, written by the M5 baking tool; in M4 a Python helper bakes them (section 10).

**Hurtboxes** — `UGSTN_HurtboxSetDefinition { TArray<FGSTN_Hurtbox { FName Name; FName Bone; FGSTN_HitShape Shape
(root-relative, baked from the reference pose); FGameplayTagContainer Tags (e.g. Combat.Hurtbox.Head, .Feet); uint8 Priority = 0;
FGSTN_HitReactionModifiers ReactionScale }>; float StaggerThreshold = −1; EGSTN_HitReaction MaxReaction = Launch }`.
`Priority` and the array index decide which hurtbox a hit lands on when several overlap (§6 step 4); `ReactionScale` (which
also carries the hurtbox's own `StaggerThreshold` and `MaxReaction`) is the "where" input of hitstun/hitstop/knockback for combat
without parts (§3); the set-level `StaggerThreshold` (negative = use the rules value) and `MaxReaction` are the **entity**
defaults, so a whole enemy type can be given a threshold without touching each hurtbox.
`FGSTN_HurtboxModifierTrack : FGSTN_TimelineTrack { EGSTN_HurtState State (Normal/Intangible/Invincible/Armor); float
ArmorThreshold; TArray<FName> AffectedHurtboxes (empty = all); FGameplayTagContainer AffectedTags (matches `FGSTN_Hurtbox::Tags`; union
with the names); TArray<FGSTN_HitShape> ShapeOverrides; }`.
Intangible = not hit at all; Invincible = hit registers (attacker hitstop, confirm) but no damage/knockback/hitstun;
Armor = no knockback/hitstun while the hit's stagger force is below `ArmorThreshold`, damage applies (*Proposal*: `ArmorThreshold`
is measured in **stagger force**, so it is a temporary `StaggerThreshold`: `HurtState` records it in the context and `HurtboxReaction`
writes the larger of the two as the effective threshold while the window is open; this settles the "damage or knockback" unit left open
for `ArmorThreshold`).
**Super armor** (owner, 2026-10-22) is the special case of **infinite stagger resistance**: `ArmorThreshold` (or a hurtbox, set or
part `StaggerThreshold`) may be `EGSTN_StaggerThreshold::Infinite` (stored as a flag, not a float sentinel, so tuning UI and validators
show it plainly): no hit ever staggers, damage and hitstop still apply. **No `Poise`** (owner, 2026-10-22: armor and poise are
special cases of stagger resistance and remaining weight): armor is a stagger threshold (permanent on a hurtbox/part, or a
temporary `Armor` window), super armor its infinite case, and "poise" is the effective weight that falls with remaining health.

**Knockback model** — `UGSTN_KnockbackModel` (abstract, EditInlineNew, const methods): `Compute(const
FGSTN_KnockbackInput&) → FGSTN_KnockbackResult { EGSTN_HitReaction Reaction; bool bStaggered; float StaggerForce; FVector
LaunchVelocity; int32 HitstunFrames; bool bTumble; int32 HitstopFrames; int32 AttackerHitstopFrames; }`. `FGSTN_KnockbackInput`
also carries the victim's `HealthFraction` (derived, §3b) and `FGSTN_HitReactionModifiers Reaction`.
`UGSTN_KnockbackModel_PercentScaling` (the default before 2026-10-22; now the platform-fighter option, the standard is
`WeightDriven` below): knockback `K` grows with the victim's damage percent and the hit's damage, scales
inversely with weight, plus base and growth (the commonly documented platform-fighter formula, every constant exposed as a
property); `K` then feeds the stagger, hitstun, hitstop and reaction-tier rules below, whose constants are shared by every model
through one struct, `FGSTN_ReactionTuning`, on the abstract base class (`HitstopPerDamage`, `HitstopBase`, `MaxHitstop` move there).
`UGSTN_KnockbackModel_Fixed` (HP games): knockback = base, no percent.
`UGSTN_KnockbackModel_CappedPercent` (owner direction, [combat-parts.md](combat-parts.md) §4.1): the percent term is
`DamagePercent` as is, used with `DamageMode::PercentCapped` (below).
**`UGSTN_KnockbackModel_WeightDriven` — the standard model** (owner, 2026-10-22, 11:18–11:20: "il peso determina la forza del
knockback"; "in smash è il peso che diminuisce all'aumentare del danno"; *Proposal* for the formula):
`K = (BaseKnockback + KnockbackGrowth × Damage) × WeightReference / EffectiveWeight`, `EffectiveWeight = max(Weight × WeightScale,
MinWeight)`; `Damage` is this hit's damage (the attack's power), and the victim's accumulated damage enters **only** through
`WeightScale` (the `HealthWeight` curve, or `PartResolve` with lost mass). `FixedKnockback ≥ 0` replaces the bracket as in the other
models; `WeightReference` (default 100, the weight at which `K` equals the authored bracket) and `MinWeight` are properties. This is the
model of the standard rules asset. `PercentScaling` and `CappedPercent` (the Smash formula, where percent is a term of its own) stay
for games that want the exact platform-fighter feel; they must then use a **flat** weight curve, and the rules validator warns when a
percent-term model and a non-flat `HealthWeight` curve meet in one pipeline (damage counted twice).

**Three dials, one job each (owner, 2026-10-22, 11:20).** The pipeline keeps them separate, and every default keeps them so:

| Dial | Victim side | Attack side | Decides | Never decides |
|---|---|---|---|---|
| **Hardness** (armour value, as Helldivers; per part with `GSTNParts`, combat-parts.md §3) | `Hardness` | `Penetration` | whether, and how much, damage applies; too hard = ricochet | hitstun, knockback |
| **Stagger resistance** | `StaggerThreshold` (hurtbox, part, entity, rules; `Infinite` = super armor) | stagger force | whether the victim enters hitstun | damage, hitstop (it applies either way unless `HitstopScale` = 0) |
| **Weight** (current: base weight × curve of damage taken × lost mass) | `Weight`, `WeightScale` | knockback (`BaseKnockback`, `KnockbackGrowth`, `FixedKnockback`) | knockback strength, so the tier (stagger in place, push back, launch), distance and hitstun length | whether damage applies, whether hitstun starts |

Cross-terms exist only as explicit designer knobs, default identity: a penetration band's `StaggerScale` and `HitstopScale`
(combat-parts.md §3.4) and a hurtbox's `KnockbackScale`. The ricochet band is the one built-in exception: a bounced hit does nothing
at all (owner, §3b item 3c).

**Stagger force, hitstun condition, hitstop and reaction tiers (owner, [game-design-direction.md](game-design-direction.md) §2b).** The model evaluates
these four rules in order, from the hit data and `Ctx.Reaction` only (all *Proposal* except what the owner stated):

1. **Stagger force** belongs to the **attack**, never to the victim. `EGSTN_StaggerForceMode::Derived` (default):
   `StaggerForce = StaggerForceScale × (Damage × StaggerPerDamage + NominalKnockback × StaggerPerKnockback)` with
   `NominalKnockback = FixedKnockback ≥ 0 ? FixedKnockback : BaseKnockback`. Both inputs are **authored** values (no victim percent,
   no weight), so the force scales with the attack's power and is the same against every victim. `Override`: the designer's
   `StaggerForce` replaces the bracket (`StaggerForceScale` still multiplies, so a relationship variant can halve it).
   `StaggerPerDamage` and `StaggerPerKnockback` are properties of `FGSTN_ReactionTuning`, defaults tuned in data.
2. **Hitstun condition (owner).** `bStaggered = !Reaction.bNoFlinch && StaggerForce × Reaction.StaggerScale ≥ Threshold` with
   `Threshold = max(0, Reaction.StaggerThreshold)`: a negative value means "unset", so a context no step touched staggers on every hit
   (the identity). The `HurtboxReaction` step resolves the threshold of **the hurtbox that was hit**: the hurtbox's own
   (`ReactionScale.StaggerThreshold`), else the entity default (`UGSTN_HurtboxSetDefinition::StaggerThreshold`), else the rules'
   (`UGSTN_CombatRules::StaggerThreshold`, default 0); the first non-negative value wins. With `GSTNParts` the part's threshold
   takes the hurtbox's place ([combat-parts.md](combat-parts.md) §4.4). An `Armor` window raises it (above). `StaggerScale` is a victim-side
   modifier (the penetration band of M4b, a hurtbox), default 1.
   **Below the threshold** the hit still does what Armor does: damage applies, **hitstop applies** on both sides (a boss that must not
   even pause sets `HitstopScale = 0` on that hurtbox), `HitReceived` is raised with `bStaggered = false`, and presentation plays a
   **flinch cue and additive animation only**. No `ActionState` change, the current move is not cancelled, no launch, no push, no hitstun.
3. **Hitstop.** `EGSTN_HitstopMode::Auto` (default): `Base = clamp(floor(Damage × HitstopPerDamage + NominalKnockback ×
   HitstopPerKnockback + HitstopBase), 0, MaxHitstop)`; the victim's `HitstopFrames` = `Base × HitstopMultiplier × Reaction.HitstopScale`,
   the attacker's `AttackerHitstopFrames` = `Base × AttackerHitstopMultiplier × Reaction.HitstopScale`, each clamped again to `MaxHitstop`.
   `HitstopPerKnockback` defaults to 0, so content that does not set it keeps the old damage-only hitstop. `Override`:
   `HitstopFramesOverride` is the victim's final value and `AttackerHitstopFramesOverride` (when ≥ 0, else the same value) the attacker's:
   the multipliers and `HitstopScale` are ignored (the designer's number is final), still clamped to `MaxHitstop` as a safety limit
   (a validator warns above it). **Which knockback** (owner, 2026-10-22: "non vedo perché il client non potrebbe predire il comportamento corretto"; this replaces
   the earlier "authored knockback only" rule): `Auto` uses the **real** `K` of step 4 and the real damage, computed from the
   **victim's reaction state as the attacker sees it**: the snapshot entry carries a compact `FGSTN_VictimReactionState`
   (damage percent, effective weight scale, broken-part mask and thresholds through the extension of §3b, active Armor/Intangible
   windows), which is replicated state the attacker's machine already has. The authority evaluates the attacker's side with the
   **same view**: lag compensation (§9) rewinds `FGSTN_VictimReactionState` together with the transforms, so the host computes the
   attacker's hitstop from exactly what the attacker saw, and the attacker's prediction matches by construction. The **victim's**
   own hitstop, hitstun and reaction are computed by the authority from the victim's **current** state; they differ from the
   attacker's view only when the victim's state changed inside the latency window (another player hit it, it broke a part, it
   entered a window). Then attacker and victim hitstop can differ by a few frames, which is accepted (they are already separate
   values); nothing is corrected on the attacker. *Proposal*; open question M4.28 records the choice.
4. **Reaction tier (owner: weight decides).** `K` is computed with `Weight × WeightScale`, so a heavier or healthier victim gets a smaller
   `K`; remaining health (`HealthWeight`) or lost parts (`PartResolve`) lower the weight, raise `K` and move the victim up the tiers.
   For a staggered hit: `Reaction = K < PushBackThreshold ? StaggerInPlace : K < LaunchThreshold ? PushBack : Launch`, then capped at
   `Reaction.MaxReaction` (victim data: a heavy enemy or armoured part that can never fly). `PushBackThreshold` and `LaunchThreshold`
   are `FGSTN_ReactionTuning` properties (validator: `PushBackThreshold ≤ LaunchThreshold ≤ TumbleThreshold`); **both default to 0, so
   every staggered hit is a `Launch`, today's behaviour**. `K` is taken after `KnockbackScale`, before `KnockbackResistance` (which
   scales the velocity at hitstop end and never changes the tier).
   **Standard rule (owner, 2026-10-22, 11:18–11:20):** the victim's **current** weight decides the knockback's strength (`WeightDriven`
   model above), and a hit launches only if that knockback reaches `LaunchThreshold`; otherwise a staggered victim is only staggered
   (in place or pushed back). The weight comes from the
   damage taken **before** this hit (the victim's state at the end of F-1, plus the damage of earlier entries of the same inbox
   drain, in the drain order of §7), so a victim that took damage is lighter for the next hit, exactly as in the owner's example. A
   victim already in hitstun is resolved by the same rule with no special case: the second hit recomputes the tier and can launch it
   or stagger it again (question M4.27). The C++ defaults of the thresholds stay 0 (identity, so unextended tests keep the
   always-launch result); the **standard rules asset** shipped with `GSTNCombat` (`GSTN_CombatRules_Standard`, the default of the
   plugin's developer settings) sets real values, *Proposal* starting points to tune: `PushBackThreshold` = 30 % and
   `LaunchThreshold` = 60 % of `TumbleThreshold`. This hit's own damage does **not** lighten the weight that decides its knockback
   (owner confirmed, 2026-10-22, 11:32, question M4.35): it counts from the next hit on.
   - `Launch`: today's behaviour, **Smash-like percent knockback kept**: `LaunchVelocity` = direction × `K`, `bTumble = K ≥
     TumbleThreshold`, DI at hitstop end, forced `Falling`.
   - `PushBack`: a **horizontal slide on the ground, with no airborne state**: `LaunchVelocity` = the horizontal direction × `min(K ×
     PushBackSpeedScale, MaxPushBackSpeed)`; no tumble, no DI.
   - `StaggerInPlace`: hitstun with **zero displacement** (`LaunchVelocity = 0`).
   `HitstunFrames = max(MinHitstunFrames, floor(K × HitstunFactor × HitstunMultiplier × Reaction.HitstunScale))` when staggered, else 0 (the
   floor keeps a stagger in place readable). The stagger force only **gates** hitstun: it does not scale its length.

**Weight, stagger and juggle (owner §2, §2b).** Every model applies the modifiers of `Ctx.Reaction` exactly once, in this order:
`Weight × WeightScale`, knockback magnitude, `× KnockbackScale`, the stagger test (force `× StaggerScale` against the threshold), the
reaction tier from the magnitude (capped by `MaxReaction`), hitstun from the knockback `× HitstunScale`, hitstop `× HitstopScale`;
`bNoFlinch` (or a failed stagger test) keeps damage and hitstop but gives no hitstun, launch or push (the Armor semantics). The
modifiers are written by steps, never by the model, so the model stays one pure function:

- `FGSTN_HitReactionModifiers { float WeightScale = 1, KnockbackScale = 1, HitstunScale = 1, HitstopScale = 1, StaggerScale = 1;
  float StaggerThreshold = −1 (negative = unset); EGSTN_HitReaction MaxReaction = Launch; bool bNoFlinch = false; }`
  (`GSTNCombatCore`; defaults are the identity, so unextended content is unchanged).
- **Weight that depends on remaining health**: a generic step `HealthWeight` (before `ComputeKnockback`), **in the standard pipeline**
  (owner, 11:18: the logic is standard and generally does not change; a game can still replace or remove the step, §3.8 of
  modularity-and-tags.md), writes `WeightScale = Curve(Progress)`
  from a baked curve (`FGSTN_BakedCurve`, 64 `uint16` entries, integer interpolation, so every machine reads the same number;
  default 1.0 at 0 % falling to 0.6 at the cap: a battered enemy flies further and climbs the reaction tiers). The curve is
  **entity data**: `UGSTN_HurtboxSetDefinition::WeightCurve`, falling back on the rules' default curve, so an enemy type that must
  never get lighter has a flat curve and needs no other change. `Progress` is read from the damage taken before this hit (step 4). `Progress = DamagePercent / MaxHealth`
  under `PercentCapped`, `1 − Health / MaxHealth` under `Health`. With `GSTNParts` the `PartResolve` step **replaces** it (it
  adds lost mass of broken parts): the rules validator reports both in one pipeline.
- **Stagger / juggle by what hits and where**: "what" = the hit's stagger force (`StaggerForceMode`, `StaggerForce`, `StaggerForceScale`),
  `HitstunMultiplier`, `HitstopMode` and the hitstop multipliers; "where" = the hurtbox the hit landed on: its `ReactionScale` (scales,
  `StaggerThreshold`, `MaxReaction`) combined with the entity defaults and the rules by the `HurtboxReaction` step (**hurtbox reaction
  and part reaction of GSTNParts must not both be set** on the same hurtbox: validator). The juggle factor of a victim already in
  `Tumble` is `AirborneJuggleScale` on the rules (default 1), the knob that allows or limits long juggles. Direction: yaw from
`YawSource` on the movement plane, pitch from `AngleDeg` (`Sakurai3D`: low angle on grounded victims below a knockback
threshold, higher in the air; `AutoLink`: towards the hitbox's velocity).

**DI** (owner direction 2026-10-22, game-design-direction.md §2c; replaces the earlier yaw-only DI) —
`UGSTN_HitMathUtils::ApplyDI(Launch, StickOnPlane, Up, const FGSTN_DIParams&)`, stateless, doubles. `Up` is the victim's
local up at `HitstopEndFrame` (`−GravityDir` from the Mover sim state; gravity can change arbitrarily, so never world Z),
and `StickOnPlane` is the stick projected on the plane normal to that `Up`. The stick is still read on the movement plane, but it now changes **both the yaw and the elevation** of the launch; the speed is unchanged.
The elevation is handled in **dot space** (`e = LaunchDir · Up`), which is how the owner gave the examples.

1. `s = |Launch|`, `e = Launch/s · Up`. Horizontal direction `h = normalize(Launch − (Launch·Up)·Up)`; if its length is
   below a small epsilon (straight up or down), `h` = the hit's `YawSource` direction. Lateral axis `r = Up × h`.
2. Stick strength `k = StrengthCurve(|Stick|) × ScaleByVictimTag` (M6 rules; M4 default: linear, 1). With `Ŝ = Stick/|Stick|`:
   `a = k × (Ŝ·h)` (**along** the launch: `+1` = DI out, `−1` = DI in), `b = k × (Ŝ·r)` (**lateral**).
3. **Yaw** (lateral stick): `h' = RotateAroundAxis(h, Up, MaxDIYawDegrees × b)`.
4. **Side**: `σ = +1` if `e > UpwardThresholdDot` (default `−0.1`: slightly downward launches behave as upward ones,
   as the owner asked), else `σ = −1` (downward launch, measured against the down vector).
5. **Elevation**: `Δ = MaxDIElevationDot × (−a)` (default `0.2`); `e' = e + σ·Δ`. DI in (`Δ > 0`) moves the launch away
   from horizontal on its side (steeper up, or steeper down); DI out (`Δ < 0`) moves it towards horizontal.
6. **DI out never crosses horizontal**: if `Δ < 0`, `e' = max(e', min(e, 0))` when `σ = +1`, `e' = min(e', max(e, 0))`
   when `σ = −1`. So a horizontal launch is not changed by DI out.
7. Clamp `|e'| ≤ MaxElevationDot` (default `0.98`, keeps a horizontal direction for the yaw).
8. `Launch' = s × (h'·sqrt(1 − e'²) + Up·e')`.

Worked examples (defaults, full stick): `e = 0.4` → DI in `0.6`, DI out `0.2`; `e = 0` → DI in `0.2`, DI out `0`;
`e = −0.5` (dot with down `0.5`) → DI in `−0.7` (down `0.7`), DI out `−0.3` (down `0.3`); `e = −0.05` → DI in `0.15`,
DI out unchanged. A lateral stick only rotates around `Up`. The owner's numbers were examples of the principle (DI in = more
vertical, DI out = more horizontal); the amounts are tuning (`MaxDIElevationDot`, the strength curves). Note the jump at the threshold: at `e = −0.09` DI in
gives `+0.11`, at `e = −0.11` it gives `−0.31` (inherent in the owner's rule; `UpwardThresholdDot` is tunable).
`FGSTN_DIParams { float MaxDIYawDegrees = 15; float MaxDIElevationDot = 0.2; float UpwardThresholdDot = -0.1; float
MaxElevationDot = 0.98; }` lives on `UGSTN_CombatRules` (*Proposal* values); M6's `FGSTN_DIRules` adds the strength
curve, the victim-tag scales and `MinKnockbackForDI`. DI is read from the stick on the frame `HitstopEndFrame` and
applies only to the `Launch` reaction tier.

**Resolution pipeline** — `UGSTN_HitResolutionStep` (abstract, EditInlineNew, const): `EGSTN_StepResult
Resolve(FGSTN_HitResolutionContext& Ctx) const` (Continue/Stop). `Ctx` holds the incoming hit, the victim's state at
end of F-1 (combat state, attributes snapshot, tags), and the outcome being built `{ bHit, bBlocked, bParried, bClank,
Damage, Knockback result, MeterGains, Events (tags), Effects to apply }`. Steps in M4: `TeamFilter`, `HurtState`
(intangible/invincible/armor), `HurtboxReaction`, `HealthWeight` (standard, §3), `ComputeKnockback` (calls the model), `ApplyDamage` (adds
`Outcome.Damage` through the damage GE), `Hitstop`, `MeterGain`. Placeholders for M6 (`Shield`, `Parry`) and M4b
(`PartResolve`, after `Shield` and before `ComputeKnockback`) are documented, not implemented. The context also holds
`FGSTN_HitReactionModifiers Reaction`, `uint8 HurtboxIndex` and `TArray<FInstancedStruct> OutcomeExtensions` (§3b).

## 3b. Generic seams for optional plugins (acceptance items of M4)

`GSTNCombat` must build, run and pass its tests with `GSTNParts` (and every other combat extension) absent. M4 therefore
lands these generic, inert-without-a-provider seams, each reviewed as a combat change in its own commit
([combat-parts.md](combat-parts.md) §1 lists them; they use the extension mechanism of modularity-and-tags.md §5b):

1. **Context modifiers and outcome extensions**: `Ctx.Reaction` (`FGSTN_HitReactionModifiers`) and `Ctx.OutcomeExtensions`
   (`TArray<FInstancedStruct>`); `PercentScaling`, `HealthFraction` (M8), `CappedPercent` and `Fixed` models read the modifiers.
2. **`HurtboxIndex`** (`uint8`, which hurtbox the overlap picked) on the victim inbox entry and on the context. Rule (*Proposal*,
   the same as combat-parts.md §3.4): among overlapped, non-intangible hurtboxes pick the highest `Priority`, ties by lowest
   index. The attacker stores it, the victim never re-derives it; lag compensation already rewinds the transforms.
3. **Outcome appliers**: `IGSTN_OutcomeExtensionApplier` (registered by type), called when the combat tick applies the outcome
   (§7) for each `OutcomeExtensions` entry whose struct type it owns. State written there is state at F+1, so rollback restores
   it with the combat state.
3c. **Attacker outcome** (owner direction, ricochet): `IGSTN_AttackerOutcomeExtension::Evaluate(const FGSTN_HitContextView&) →
   EGSTN_AttackerHitOutcome { Normal, Ricochet }`, called on the attacker side at §6 step 6 with the snapshot's victim reaction
   state, deterministic. `GSTNCombat` alone always returns `Normal`. On `Ricochet` the attacker's `UGSTN_GA_BaseCombat` ends its
   timeline at that frame and runs its `FGSTN_RicochetResponse { TSoftObjectPtr<UAnimMontage> RecoilMontage; int32 RecoilFrames
   (locked part; the rest of the montage is a cancellable tail, §7);
   FGSTN_MovementTrack Movement (optional push-back of the attacker); FGameplayTag CueTag; }`, authored **per attack** on the
   timeline, with a fallback on the weapon then the kit (*Proposal*); the recoil is predicted like any timeline (montage synced to
   the combat clock, M3) and confirmed by the host from the same rewound view (§9).
4. **`DamageMode::PercentCapped`** and `UGSTN_KnockbackModel_CappedPercent` (§3).
5. **`FGSTN_HitData::ExtensionData`** and its per-field override variant (`FGSTN_HitDataOverride` extension entry), so a
   relationship variant can change an extension's fields.
6. **Tick-extension point "before candidates"** may return a `HitGroupMask` that masks the entity's own active hitbox
   groups (§6 step 1).
7. `FGSTN_HitVictimView` and the snapshot-extension POD of M8 §3 (if M8 lands first they exist already).

Without a provider every seam is the identity: no extension, modifiers at the identity (scales 1, threshold unset, no reaction cap), mask empty. Automation: a pipeline with fake
steps that write modifiers and an applier that records calls, run with no plugin present.

**Blueprint forms of the seams** (owner, 2026-10-22; rules in modularity-and-tags.md §3.8). `UGSTN_HitResolutionStep`,
`UGSTN_KnockbackModel` and the relationship resolver are `Blueprintable, EditInlineNew`, and their entry point is a
`BlueprintNativeEvent` that takes a const context view and returns a result struct, with the C++ behaviour as the default.
Outcome appliers and the attacker outcome extension get Blueprintable `UObject` adapters (`UGSTN_OutcomeApplier`,
`UGSTN_AttackerOutcome`, instanced in the rules asset), so a designer adds a new outcome (a shatter on a crystal enemy, a
recoil that disarms) without C++. These hooks are **pure by contract**: everything they may read is in the view, they touch no
world, actor or component, so they rerun safely in a resimulation. Per-frame tick extensions stay C++ by default (their cost is
multiplied by entities and resimulated frames); a Blueprint tick hook is opt-in and profiled.

**Combat rules** — `UGSTN_CombatRules` (DataAsset): `Pipeline (TArray<UGSTN_HitResolutionStep*> Instanced)`,
`KnockbackModel (Instanced)`, `DamageMode (Percent/Health/PercentCapped)` (`PercentCapped`: `MaxHealth` is the cap in percent points; `DamagePercent` is
clamped at it and reaching the cap raises `Combat.Event.KO`; `Health` unused; owner §2). **Death conditions** (owner, 2026-10-22):
health at zero (the cap reached) **or** entering a **KO zone** (a ravine, a pit, a blast-zone volume): `AGSTN_KOZone` (volume, in
`GSTNCombat`, no project dependency) raises the same `Combat.Event.KO` with `KOReason = Zone` when the entity's simulated position
enters it, read from the Mover sim state in the combat tick (so it is predicted and rolled back like any state, never from an
overlap callback); `KOReason { Health, Zone }` is on the event, `bFriendlyFire`, `HittableRelationships`
(`FGameplayTagContainer`), `RelationshipResolver (Instanced)`, `bUseLagCompensation`, `MaxRewindMs`, DI tuning (SDI: M6), `StaggerThreshold` (the global default of the hitstun condition, default 0 = every hit staggers)/`AirborneJuggleScale`, `RulesExtensions`
(`TArray<FInstancedStruct>` of base `FGSTN_RulesExtension`, §5b).
Selected per game mode (`AGameModeBase` subclass property or a GameState component `UGSTN_CombatRulesComponent`,
replicated by asset reference).

## 4. GAS side (`GSTNCombatGAS`)

- **`UGSTN_CombatAttributeSet`**: `Health`, `MaxHealth`, `DamagePercent`, (no `Poise`: §3 Armor),
  `OutgoingDamageMultiplier`, `IncomingDamageMultiplier`, `KnockbackResistance`, `HitstunResistance`.
- **`UGSTN_GE_Damage`** (instant) with **`UGSTN_DamageExecution`**: reads SetByCaller `Combat.SetByCaller.Damage`, the
  instigator's outgoing and the target's incoming multipliers; writes `DamagePercent` (+) or `Health` (−) by
  `DamageMode`.
- **`FGSTN_HitEffectContext`** (`FGameplayEffectContext` subclass with `NetSerialize`): hit data id (move + hitbox
  index), hit location, relationship; used by cues.
- Meter gain GEs (`UGSTN_GE_MeterGain`, SetByCaller per meter).

## 5. World subsystem and snapshot (`GSTNCombatNP`)

- **`UGSTN_CombatWorldSubsystem`**: registry of hittable entities `{ int32 EntityId (NP proxy id); TWeakObjectPtr
  component; FGenericTeamId Team; const UGSTN_HurtboxSetDefinition*; }` (registration on BeginPlay, removal on EndPlay,
  sorted by id).
- **Per-step snapshot**: `{ int32 Frame; per entity: transform and velocity at end of F-1 (tracker sync state), facing,
  hurt state and active hurtbox overrides from its combat sync state at end of F-1, team, `bInHitstop`, action state }`
  plus a uniform-grid broadphase (cell size = 2 × the largest hurtbox extent, default 400 cm).
- **Built by the first combat tick of a step**: a combat tick at frame F rebuilds the snapshot when `Snapshot.Frame !=
  F` or the snapshot was invalidated. Every combat component's `RestoreFrame` (called on rollback for every instance)
  invalidates it, so a re-simulated frame never reuses a snapshot built before the rollback. At the first tick of a step
  no combat instance has ticked F yet, so every entity's state read is end of F-1 (fact 8 of combat.md §2).
- Entities that do not tick on this machine (interpolated proxies) contribute their interpolated presentation state:
  on a client this is the attacker's view of the world, which is what it predicts against.

## 6. Attacker side (combat tick at F)

1. Active hitboxes of the current move frame (paused frames excluded; groups in the `HitGroupMask` given by the tick-extension
   point "before candidates" are skipped: no hit, no hitstop, no confirm) → world shapes: `Root` = end-of-F-1 actor
   transform × local key; `BakedSocket` = transform × baked frame; `WorldAtMoveStart` = `MoveOriginTransform` (new sync
   field, quantized, set when the move starts); `Projectile` = owned by the projectile system (later).
2. Broadphase → candidates (excluding self unless the variant for `Self` exists).
3. **Relationship** first (resolver: self / same team = Ally / `FGenericTeamId::NoTeam` = Neutral / else Enemy), then
   `HittableRelationships` and `bFriendlyFire`, then the variant (`bIgnore`, `ShapeScale`).
4. Overlap (swept if flagged) against the target's hurtboxes with its overrides; `Intangible` skipped; the hurtbox that wins
   the `Priority`/index rule of §3b becomes `HurtboxIndex`. If several hitboxes of the same `HitGroup` touch the same target this
   frame (sweet and sour spots), only the one with the highest `Data.Priority` (ties: lower index) produces the hit.
5. **Hit groups**: `HitTargets` ring in the combat sync state (8 entries `{ EntityId, TimelineStartFrame, HitGroup }`) — a
   target is hit once per group per move execution.
6. For each new hit: record it; **predict own consequences**: first the **attacker outcome** (`EGSTN_AttackerHitOutcome`:
   `Normal`, or `Ricochet` when an extension says so, §3b; with `GSTNParts` a too-hard part); on `Ricochet`: no hitstop, no
   confirm, no meter, and the ability's ricochet response (§3b) instead; otherwise hit-confirm flags (`OnHit`, or `OnBlock` when the
   snapshot says the victim is shielding — always false until M6), attacker hitstop window from the knockback model's
   `AttackerHitstopFrames` (computed from the hit and the **victim reaction state as seen in the snapshot**, §3 hitstop), attacker
   meter gain request; deliver the hit to the victim (7) in both cases (a ricochet still informs the victim for cue and event).

## 7. Victim side

- **Inbox** on the victim component: `{ int32 Frame; int32 AttackerId; resolved FGSTN_HitData (variant applied);
  FVector HitLocation; FVector HitboxVelocity; float AttackerYaw; FGameplayTag Relationship; uint8 HurtboxIndex; }`, written only when the
  victim instance ticks on this machine (authority, or locally controlled). Drained at the victim's combat tick of
  `Frame + 1` (entries with `Frame < F`), in (AttackerId, hitbox order) order. On rollback the inbox drops entries with
  `Frame ≥` the restored frame (they are re-delivered by the attackers' re-simulation).
- **Resolution** (combat tick at F+1): run the pipeline per entry. Outcome applied as state and requests. **Staggered**
  (`bStaggered`, §3): `ActionState = Hitstun` (or `Tumble` for a tumbling `Launch`), `HitstopStartFrame = F`, `HitstopEndFrame = F +
  HitstopFrames`, `HitstunEndFrame = HitstopEndFrame + HitstunFrames`, `PendingReaction` (`EGSTN_HitReaction`) and `PendingLaunch`
  (quantized velocity: zero for `StaggerInPlace`, horizontal for `PushBack`), cancel of the current move (`ActiveTableIndex =
  INDEX_NONE`; GA_Move ends itself). **Not staggered** (stagger force below the threshold): only the hitstop window and the freeze
  (no `ActionState` change, no `PendingReaction`, the current move resumes after the freeze, which restores the incoming velocity)
  and the flinch cue. In both cases the registered outcome appliers run for each `OutcomeExtensions` entry (§3b), and the requests are:
  damage GE, extra GEs, cue, freeze move (duration = hitstop), buffer event `Input.BufferEvent.HitReceived`, gameplay event
  `Combat.Event.Hit.Received` whose payload carries `Reaction` and `bStaggered` so presentation picks the montage (reaction abilities may
  listen; they must not set combat state). **`PushBack` needs ground** (*Proposal*): a victim that is airborne or in a wall mode at the
  end of F-1 (read from the Mover sim-state tracker, the source of the snapshot) is promoted to `Launch`, since there is nothing to
  slide on; `StaggerInPlace` works in every mode.
- **Hitstop end** (frame `HitstopEndFrame`), by `PendingReaction`. **Launch**: DI from this frame's stick → `ApplyDI` → request
  knockback move (`FGSTN_LayeredMove_Knockback { Velocity; float DecayPerFrame; float GravityScale; int32 ElapsedFrames; }`, forcing
  Falling, applied through a GE + move creator) and `KnockbackResistance` applied. **PushBack**: request `FGSTN_LayeredMove_PushBack {
  FVector Velocity (horizontal); float DecayPerFrame; int32 ElapsedFrames; }` through the same GE + move creator: it keeps the current
  ground mode (no `Falling`, no tumble, no DI), the ground snap stays on, it stops at a wall or a ledge (no splat, no bounce, no fall:
  *Proposal*), and ends when the speed reaches zero or at `HitstunEndFrame`; `KnockbackResistance` scales the speed.
  **StaggerInPlace**: no move; the freeze ends with zero horizontal velocity (§8).
- **SDI is not in M4.** It is specified once, by M6 §5.5 (`FGSTN_SDIRules`; owner direction: SDI is a **held** direction that
  drifts the victim during hitstop, game-design-direction.md §2c). M4 ships DI
  only; the sync field `SDICount` is reserved here and written by M6. (Earlier drafts described a basic SDI here while
  §1 put it out of scope: M6 owns it.)
- **Hitstun end**: `ActionState = Actionable` (or `Tumble` until landing when tumbling; `PushBack` and `StaggerInPlace` go straight
  to `Actionable`); M6 adds tech.
- **Cancellable tail** (owner direction 2026-10-22, game-design-direction.md §6c): hitstun is the **locked** part of the reaction;
  the reaction animation goes on after it as a **cosmetic tail that anything cancels** (any command, movement or jump input), the
  same pattern as the first actionable frame of every timeline (M3 `FirstActionableFrame`). For staggers the owner's reference is
  "about half a second unskippable, then cancellable": *Proposal*: `UGSTN_CombatRules::MinStaggerLockFrames` (default 31 frames,
  ~0.5 s at 62.5 Hz) is the floor of `HitstunFrames` for `StaggerInPlace` and `PushBack`, overridable per entity or part; the
  reaction montage is authored longer and its tail is blended out on the first input after `HitstunEndFrame`. The same applies to
  the ricochet recoil (§3b item 3c): `RecoilFrames` is the locked part, the rest of the recoil montage is a cancellable tail; so the
  attacker is punishable only during `RecoilFrames`.
- Tags projected: `Combat.State.Hitstun`, `Combat.State.Tumble`, `Combat.State.Hitstop`.

## 7b. Surface impacts: wall splat, bounce, destruction

Owner direction (§4): a launched entity that hits a surface can **splat** on a wall, **bounce**, or cause **destruction**, "as
the designer decides". M4 supplies the mechanism and the data; the *who decides* question is a *Proposal*.

**Outcomes** (`EGSTN_ImpactOutcome`): `None` (slide or land as M4 does today), `Splat` (pinned to the surface for
`SplatFrames`, velocity zero, extra damage and stun), `Bounce` (velocity reflected about the contact normal × `Restitution`,
stays `Tumble`), `BreakSurface` (the surface object is destroyed, the victim continues with `ResidualSpeedScale` of its speed).
*Reading to confirm:* "destruction" may also mean the **victim** shattering on impact; modelled as an optional `ImpactDamage`
that can reach the cap (KO), not as a fourth outcome.

**Who owns the choice (*Proposal*, three layers, first match wins):**

| Order | Owner | Data | Decides |
|---|---|---|---|
| 1 | **Hit data** | `FGSTN_ImpactProfile { EGSTN_ImpactOutcome Force; FGameplayTagContainer Forbid; float SpeedScale }` on `FGSTN_HitData` | A move can force an outcome (wall-slam finisher) or forbid one (a light jab never splats) |
| 2 | **Victim** | `FGSTN_ImpactResponse` from the kit / definition (by `FGSTN_KitExtension` or the rules): per surface type outcome allowed, `MinImpactSpeed`, `Restitution`, `SplatFrames`, `ImpactDamageScale`; heavy enemies never splat | What this entity can undergo |
| 3 | **Surface** | `UGSTN_SurfaceImpactTable` (DataAsset on the rules): **surface type → outcome defaults and thresholds**; surface type = the contact's `UPhysicalMaterial` mapped to a native/ini tag `Surface.Type.*` (`Surface.Type.Wall`, `.Glass`, `.Crate`); an actor can override it with the tag on its component (`IGSTN_ImpactSurface`, also the hook that destroys a `BreakSurface` target) | The physical truth of the wall: glass breaks, concrete splats, a trampoline bounces |

Rule: `Forbid` removes outcomes; `Force` (if the surface allows anything at all, or `bForceIgnoresSurface`) wins over the rest;
otherwise the victim's response restricts the surface's default. The impact speed along the normal must exceed `MinImpactSpeed` of
the chosen row, else `None`. Pure function `UGSTN_ImpactMath::Resolve(const FGSTN_ImpactInput&, rules) → FGSTN_ImpactResult`,
integer-quantised inputs, golden-tested in Core.

**Where it runs.** The probe is the contact record of the tumble (`FGSTN_TumbleContactSyncState`: kind, normal, impact speed,
contact frame, **plus** `SurfaceTypeIndex`, `uint8`, from the physical material through a baked table): M4's knockback layered
move carries a minimal probe that writes it; M6's `FGSTN_TumbleContactModifier` (tech) is the same record, not a second one.
The combat tick reads it at end of F-1 (one frame after contact, as M6 §6) and applies the result as state and requests
(a freeze or a reflect velocity through Mover queued moves, damage GE, request to `IGSTN_ImpactSurface`, event
`Combat.Event.Impact.*`). The surface table is part of the rules asset and the physical-material mapping is baked at load;
nothing reads live components in the decision.

**Trampolines** (owner, 2026-10-22: "tieni conto della presenza di eventuali trampolini"). *Proposal*: a trampoline is a
level object (`AGSTN_Trampoline`, in a small optional plugin `GSTNLevelObjects`, not in `GSTNCombat`) that plays two roles:
(1) for a **tumbling** victim it is just a surface whose `Surface.Type.Trampoline` row forces `Bounce` with its own
`Restitution` and a minimum outgoing speed along its normal, through `IGSTN_ImpactSurface` (no special case in the impact
code); (2) for **anyone not tumbling** (players, enemies, minions, thrown objects) landing on it, it launches along its
normal with a fixed or speed-scaled velocity: a Mover launch requested through the normal queued-move path (players and
fighters: `FGSTN_LayeredMove_Launch` via GE, predicted like any jump; minions: the lightweight movement's launch; M10
projectiles: a bounce of the analytic segment). The launch direction is the trampoline's normal, not gravity's up, so a
trampoline on a wall or ceiling works with any gravity. Interaction with the variable jump (holding jump for a higher
bounce) and with fast fall is an open question (M4.31).

**Interaction with tech (M6).** At the same contact frame the tech window is evaluated first: a **successful tech cancels the
outcome** (no splat, no bounce, no damage; a wall tech is the reward). Only after a failed or absent tech does the impact
resolve, and its result **replaces M6's default failure branch** (ground knockdown, wall bounce-off) when the three layers
give a non-`None` outcome; with no data the M6 defaults apply unchanged. Hit data may also set `bNoTech` to make an impact
unavoidable (*Proposal*, unset by default). Splat ends in `Actionable` after `SplatFrames` (no tech from a splat in the first
version).

## 7c. Demolition force (owner, 2026-10-25 19:22)

As in Helldivers 2, some objects break only under a **minimum demolition force**. It applies to **props and level meshes** and
generally **not to enemies**: enemies and their parts keep hardness against penetration ([combat-parts.md](combat-parts.md) §3.4).
*Proposal* (checkpoint H14, after H13):

- **Attacker side**: `FGSTN_HitData::DemolitionForce` (`int32`, default 0). Designer scale in steps of 10 (for example 10 a punch,
  20 a heavy swing, 30 the drill, 40 an explosive; question DEM.1). Kit mods scale it like other hit data (M11). The drill carries
  it on its tip hit data ([spin-drill.md](spin-drill.md) §13b, PR #15).
- **Object side**: `UGSTN_DestructibleComponent` in the optional plugin `GSTNLevelObjects` (Blueprintable). It makes any actor a
  **hittable entity** with a hurtbox set, registered like other victims, plus `FGSTN_DestructibleResponse { int32 DemolitionThreshold;
  int32 HitPoints = 1; soft references to the broken version and the destruction effect; FGameplayTagContainer Tags; }`.
- **Rule** (pure, integer, in `GSTNCombatCore`): a hit **damages the object only if `DemolitionForce >= DemolitionThreshold`**; each
  such hit removes one hit point. Below the threshold the object takes nothing and plays a refusal cue
  (`GameplayCue.Combat.Demolition.Refused`, a dull clank); the drill bounces off as not drillable (spin-drill.md §7.6). Threshold 0
  means any hit counts.
- **Impacts** (§7b): `BreakSurface` also needs the impact's demolition force to reach the object's threshold. The impact force is
  `FGSTN_ImpactProfile::DemolitionForce` of the hit that launched the victim (default: that hit's `DemolitionForce`), so the designer
  decides which launches smash a crate with a body (question DEM.3).
- **Enemies**: entities ignore demolition by default. A part can opt in as data (`bUsesDemolition`, for example an armour plate
  that only explosives break).
- **Network**: destruction is a victim reaction, server-authoritative like every victim (combat.md §12.7). The attacker's machine
  predicts the hit feedback; the object's collision goes away when the confirmed destruction event arrives, so a character standing
  on a crate that breaks gets one correction. *Unsure*: how visible that is; the harness scenario of H14 measures it.
- Tags (native, `GSTNLevelObjects`): `Combat.Event.Destructible.Destroyed`, `GameplayCue.Combat.Demolition.Refused`.

## 8. Hitstop for both sides

- Move clock: `MovePausedFrames++` on every hitstop frame.
- Buffer: presses during hitstop are recorded; the time-buffer age excludes hitstop frames — pass the last hitstop window as
  `FGSTN_MatchParams::PausedStart/PausedEnd` (already in M1).
  The resolver does not run during hitstop.
- Montage: paused while a montage plays (M0 rule), resumed at `HitstopEndFrame`.
- Mover: `FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity, keeps the incoming velocity in its state and restores it
  on finish for the attacker and for a victim that was not staggered; a staggered victim's knockback or push replaces it, and a
  `StaggerInPlace` victim's freeze ends with zero horizontal velocity, a `bZeroVelocityOnFinish` flag), plus the `Combat.Mover.Frozen` tag (spike 2.2).
  `FGSTN_LayeredMove_MoveTrack` (M3) does not advance while frozen.
- **Wall lock** (project, by extension point, per wall-movement.md): `UGSTN_WallMovementModeBase` gains a protected
  virtual `IsMovementLocked(const FMoverTickStartData&)` (default: Mover has `Combat.Mover.Frozen`); when locked the
  controlled wall mode skips `CheckForWallExit`, `TryWallJump`, `TryDetachByPullingAway` and does not advance
  `TimeWithoutWall`/`PullAwayTime`. **Tell the user**: this touches the wall movement core (a new extension point, no
  behaviour change when unlocked).

## 9. Lag compensation and presentation

- **Authority, attacker controlled by a remote client** (the host machine of a listen server, or a dedicated server),
  `bUseLagCompensation`: before building its hit queries the attacker
  calls `UNetworkPredictionWorldManager::RewindActors(Attacker, interpolation time of its last input)` and builds the
  target transforms from the rewound actors (a per-attacker snapshot overlay, not the shared snapshot), then
  `UnwindActors()` before returning. Requires `UNetworkPredictionLagCompensation` on hittable pawns. AI attackers never
  rewind.
- **Victim reaction state in the rewind** (owner direction on prediction, §3 hitstop): the lag-compensation history keeps each
  hittable entity's `FGSTN_VictimReactionState` next to its transform (a few bytes per entity per frame, same window as
  `MaxRewindMs`), so a rewound query sees the victim's percent, weight and parts as the attacker saw them.
- **Predicted victim reaction (presentation).** With the same `FGSTN_VictimReactionState` the attacker's machine computes the
  victim's expected reaction (tier, hitstun and hitstop frames, launch direction without DI) and the presentation component plays it
  on the interpolated puppet right away: freeze, flinch or launch pose, and a short cosmetic extrapolation of the launch until the
  authoritative motion arrives, then a blend onto it. The victim's simulation itself stays authoritative on the host and
  interpolated on other machines (combat.md §12.7); simulating remote victims (forward prediction) is a later option, open
  question M4.16, blocked today by the proxy-attribute bug noted in combat.md §12.7.
- **`UGSTN_HitPresentationComponent`** (attacker's machine): when `FinalizeFrame` sees a new predicted entry in
  `HitTargets`, it plays local-only effects on the (interpolated) victim: flash material parameter, local cue
  (`ExecuteGameplayCueLocal`), victim mesh pause for the hitstop length. When a later corrected state removes that
  entry, it stops them early. The real cue comes from the victim's ASC and is deduplicated by hit id on the attacker's
  machine.

## 9b. Listen server (owner direction, game-design-direction.md §6b)

M4's "server" means **the authority**. The reference topology is a **listen server**: a friend hosts and plays, the others are
clients. The simulation code does not change (no topology branch, rule 1 of CLAUDE.md); what changes is what we assume about
the authority and how we measure it. A dedicated server stays supported.

- **The host is the authority and a zero-latency player.** Hit resolution for everyone, **lag compensation** (`RewindActors`) and
  the victim inbox of every non-owning entity run **on the host's machine**, in the same frame as the host's own simulation,
  rendering and input. The host's own pawn is both attacker and victim without correction: its hits resolve the frame they
  happen against current authoritative state, and hits on it arrive as ordinary inbox entries, not as client corrections.
- **Host advantage (stated).** The host never waits an RTT, never rolls back for hits, and never needs to be rewound for; every
  client attacker is partly compensated (`MaxRewindMs`) but still acts about one RTT after the host sees it. *Proposal*:
  (a) accept it in the first version, since lag compensation already moves the advantage toward the clients; (b) keep
  `MaxRewindMs` as the fairness knob, tested in the harness with the host as attacker and as victim; (c) add the presentation-only
  `bHostHitFeelParity` of combat-parts.md §5.5b only if playtests show it. No simulation delay for the host (it would break
  determinism and the host's own prediction).
- **Budget shares the frame.** Hit resolution, snapshot building, rewinds and (later) minion simulation (M8) share the host's frame
  with its own game and rendering; keep the per-hit cost small and measured on the host configuration, not on a bare server.
  The host's upload (home connection) is the bandwidth limit of the whole session; the M4 sync additions are small (§11), but the
  harness reports host bytes per second.
- **If the host leaves, the session ends** (no host migration unless the owner asks).
- Spike 2.1 and the harness cover both configurations: **listen-server (host + 2 clients)** and dedicated + 2 clients. Host as
  attacker (clients show about one RTT/2 + interpolation delay later), client as attacker against the host (the host's victim is
  authoritative: expected zero rollbacks on the host, one correction on nobody), client against client (as before).

## 9c. Movement collision and hit collision (owner, 2026-10-25 19:26)

Movement and hits already use separate geometry. **Movement** is Mover's capsule against the level's simple collision. **Hits** are
hitbox shapes against hurtbox shapes, pure maths with no world query: the M4 and M4b code on `feature/gas-npp-5.8` has no trace or
sweep in `GSTNCombat` or `GSTNParts`. So today a hitbox ignores walls (a swing hits through a thin wall), and a hurtbox inside
another one can be reached through it. The owner's edge cases need two additive, opt-in, deterministic rules (A, B) and some project
config (C):

| Case | Character | Bulky hitbox (hammer) | Thin hitbox (stab) | Projectile | Decided by |
|---|---|---|---|---|---|
| Grate or fence with small holes | blocked | blocked | passes if it fits | passes | A |
| Slit smaller than the capsule | blocked | blocked | passes | passes | A |
| Hollow medium-hardness cavity inside a heavy-armour leg | (not a passage) | hits the armour | hits the cavity through its opening | hits the cavity through its opening | B |
| Invisible wall | blocked | passes | passes | passes | C |

**A. World occlusion with a pass radius** (checkpoint H16):
- `FGSTN_HitboxSpec` gains `bool bOccludedByWorld = false` (false keeps today's behaviour), `float PassRadiusCm = 0` (0 = derived:
  sphere radius, capsule radius or the box's smallest half-size) and `EGSTN_OcclusionAnchor OcclusionAnchor` (`PreviousFrame`,
  `AttackerRoot`, `Socket`).
- After the shape overlap, a candidate hit of such a hitbox counts only if a **sphere sweep of radius `PassRadiusCm`** from the anchor
  to the contact point is clear on the hit-occlusion channel, against **complex** collision. Size alone then decides: a 30 cm hammer
  stops at a grate with 5 cm holes, a 2 cm stab or a 1 cm bullet goes through, and no level object needs a special flag.
- Cost: one sweep per candidate pair of a flagged hitbox, capped per attacker per frame (*Proposal*: 8; extra candidates are
  refused and counted in the stats). It runs in the combat tick against static level geometry, which is the same on every machine.
  *Unsure*: complex sweeps can disagree near an edge between machines; the victim is server-authoritative (combat.md §12.7), so a
  disagreement costs one correction. Destructible geometry (§7c) is seen as each machine knows it.
- Projectiles (the `Projectile` space here, thrown objects in M10) already trace the world as they move; they use the projectile
  channel with their own radius, so a bullet passes holes that a thrown crate does not.

**B. Hurtbox openings** ([combat-parts.md](combat-parts.md) §3.4, M4b checkpoint P18): `FGSTN_Hurtbox` gains an optional
`FGSTN_HurtboxOpening { FGSTN_HitShape Mouth; float MaxPassRadiusCm; }` (mouth root-relative, at the hole). A hurtbox with an opening
counts as overlapped only if the hitbox also overlaps the `Mouth` **and** its pass radius is at most `MaxPassRadiusCm`; otherwise the
selection falls to the next candidate by `Priority` (the armour). Pure shapes, no world query.

**C. Channels and profiles** (project config, no plugin names a channel):
- The combat developer settings gain `HitOcclusionChannel` and `ProjectileChannel`, default `ECC_Visibility`, so the plugin works
  alone. The project declares two trace channels in `Config/DefaultEngine.ini` (`GSTN_HitOcclusion`, `GSTN_Projectile`) and points
  the settings at them.
- Level meshes keep both simple collision (movement) and complex collision (hits and projectiles): "Use Complex Collision As Simple"
  stays off on meshes with holes. Example profiles: `GSTN_InvisibleWall` (blocks pawns only), `GSTN_BulletproofGlass` (blocks pawns,
  occlusion and projectiles), `GSTN_Grate` (simple box blocks pawns; complex mesh with holes for hits and projectiles).
- Hurtboxes and hitboxes are never physics bodies, so they never block movement; Mover's capsule never answers the hit channels.
- Questions COL.1–COL.2.

## 10. Content, tools, harness

- Hurtbox set for the test character (manual capsules, or a Python helper that reads the Physics Asset bodies and
  writes the set) and a move with: a root hitbox, a baked-socket hitbox (Python helper samples the montage at the NPP
  tick rate and writes `BakedSockets` — replaced by the M5 tool), a `WorldAtMoveStart` hitbox, a variant for allies
  (half shape, no hitstun).
- Debug draw (`gstn.Combat.Debug.Hitboxes 1` / `Hurtboxes 1`): active hitboxes per frame, colour by group; hurtboxes by
  state.
- Harness scenarios (`gasnpp_test.py`; both clients' pawns placed facing each other by a helper):
  `scenario_hit_basic` (c1 jabs c2: c2 damage/knockback/hitstun equal on all machines, c1 predicted hitstop and
  cancel-on-hit), `scenario_hit_trade` (both on the same frame), `scenario_friendly_fire` (same team: off → no hit,
  on → variant applied), `scenario_hitstop_montage` (forced reconcile during hitstop with a montage),
  `scenario_hit_on_wall` (victim wall sliding, frozen during hitstop), `scenario_di` (victim holds a side: launch
  rotated), `scenario_hit_host` (host attacker, host victim, client against host: states equal, host without rollbacks),
  `scenario_stagger_threshold` (c1 hits a high-threshold hurtbox with a light then a heavy attack: the light one gives damage and
  hitstop, no hitstun, the victim's move is not cancelled; the heavy one gives hitstun; equal on all machines),
  `scenario_reaction_tiers` (the same hit on victims of three weights: stagger in place with zero displacement, a push back that
  stays grounded and never enters `Falling`, a launch with tumble),
  `scenario_dodge_feet` (base ground dodge: feet intangible only in the initial part, so feet missed early in the leap, feet hit late, torso hit throughout), `scenario_impact_wall` (splat, bounce, forced
  and forbidden by hit data; tech cancels it in M6), all with 60/15 ms, with and without 2 % loss, **in the listen-server
  configuration (host + 2 clients) and in dedicated + 2 clients**. Clean = snapshots equal (combat state, attributes, positions
  after the launch settles) and no reconciles besides the forced ones **and the expected one per hit on the victim's
  client** (spike 2.1).
- Automation tests (Core): `UGSTN_ImpactMath` precedence (hit, victim, surface, thresholds), modifier order in every knockback
  model, stagger force (`Derived` against hand values, `Override`, scale), the hitstun condition at the boundary (force equal to the
  threshold staggers, one unit below does not; threshold resolution hurtbox → entity → rules; an Armor window raises it), hitstop `Auto`
  against hand values and clamp (`Override` is final), attacker hitstop independent of every victim field, reaction tier boundaries and
  the `MaxReaction` cap, a lighter weight never gives a milder tier, the 0/0 defaults reproduce the old always-launch results, the
  owner's two-hit example (hit 1 staggers and damages; hit 2 with the same move, against the weight after hit 1, still below
  `LaunchThreshold`: staggered again; a stronger hit 2 launches), two hits in one inbox drain see each other's damage in drain order, the
  `HealthWeight` curve table and a flat entity curve, seams with fake providers (§3b), every shape pair (touching, separated by ε, contained), swept overlap catches a fast box,
  knockback model values against hand-computed ones, DI rotation limits, variant application, pipeline step order
  with fake contexts, inbox ordering and rollback trimming.

## 11. Sync state additions

`MoveOriginTransform` (quantized), `HitTargets[8]`, `HitConfirmFlags`, `HitstopStartFrame/EndFrame` (now real),
`HitstunEndFrame`, `PendingLaunch` (int16 × 3, cm/s / 4), `PendingReaction` (`EGSTN_HitReaction`, 2 bits, packed with the existing
flag bits; state at F+1 like `PendingLaunch`, read at hitstop end to choose the move), `SDICount` (reserved, written by M6), `ImpactEndFrame` (splat),
hurt state of the current frame (derived, not
stored). Idle cost stays ~0 thanks to end-frame stamps.

## 12. Acceptance criteria

1. Spikes recorded; scenarios of section 10 clean as defined in both topologies; M1–M3 tests and scenarios still clean.
2. A hit costs the victim's client at most one rollback in the basic scenario.
3. The test move's three hitbox spaces hit where the debug draw shows them, on the authority (host) and on clients.
4a. The seams of §3b exist and are inert: combat tests and scenarios pass with no optional plugin installed; modifiers change
    knockback, hitstun and hitstop exactly once (Core tests).
4b. The base ground dodge (§1b) makes only the feet intangible, only in its initial part, and the window is kit data; surface impact outcomes follow the precedence of §7b.
4c. Hitstun follows the stagger force against the threshold of the hurtbox hit (hurtbox, entity, rules); hitstop is automatic or the
    designer's override; the reaction tier follows the weight-driven knockback and `MaxReaction` (Core tests, `scenario_stagger_threshold`,
    `scenario_reaction_tiers`).
4. Friendly-fire variants work per game mode by changing only the rules asset.
5. combat.md updated (§12, §17).

## 13. Risks

- Victim latency in PvE (interpolated enemies react ~RTT/2 + interpolation delay later): presentation must carry the
  feel; measure in spike 2.1 with presentation on.
- Forward prediction of attackers (combat.md §12.7) stays off in M4. Keep the victim inbox able to be drained on the
  owning client later (no server-only assumptions inside the resolution steps besides where the instance ticks), and
  log in spike 2.1 how many hits on the local player arrive as corrections: that number decides a later forward
  prediction milestone.
- Snapshot invalidation relies on `RestoreFrame` being called for every combat instance on rollback (true for the fixed
  rollback service, combat.md §2 fact 8); assert it in development builds.
- Swept overlaps multiply query cost; keep them opt-in per hitbox.
- **Host-only costs and advantage** (§9b): resolution, rewinds and rendering share one machine; host hits feel instant.
- **Seams slipping out of M4** make M4b edit the combat core (combat-parts.md §8.5): they are acceptance items (§12).
- **Reaction constants explosion** (stagger force, thresholds, tiers, hitstop, per hurtbox/part/entity): defaults are the identity, thresholds
  live on the hurtbox set and the rules, and a validator prints, per victim weight, which hits stagger and which tier they get.
- **Impact data explosion** (surface types × victims × hits): defaults live in the surface table, victims only restrict, hits only
  force or forbid; a validator prints the resolved outcome matrix.
