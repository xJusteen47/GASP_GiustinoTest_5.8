# Pushbox plan: ECB for the world, pushbox for fighters (design, 2026-11-15)
Base: `claude/integ-dropkick-capsule` a95e17d4 (Body Capsule track + `FGSTN_BodyCapsuleModifier`). Owner agreed (thread 22:03): one capsule doing
two jobs is the root problem. Today Mover's capsule (ECB) blocks other pawns, so a torso-fitted dair capsule landing on a victim's rounded
capsule is rigidly depenetrated ~70 cm back. Target (Smash-like): ECB = terrain only; pushbox = fighter vs fighter, soft and capped.

## 1. The ECB ignores other fighters (no project settings, no channel change)
- On registration in `UGSTN_CombatWorldSubsystem::RegisterEntity` every fighter's Mover capsule calls `IgnoreActorWhenMoving` on every other
  registered fighter, both ways; undone on `UnregisterEntity`. Mover's sweeps, floor checks and depenetration all build their params through
  `InitSweepCollisionParams` (`MovementUtils.cpp` 544/897-964), so they skip the other fighter. `ECC_Pawn` stays as is: pawn-type platforms,
  minions and the depth marker exclusion are untouched.
- Risk found: `GSTN_LinkPresentationComponent.cpp:278` un-ignores its previous link partner, which would re-enable fighter blocking. Fix: a
  small ref-counted helper (`GSTNMoveIgnore::Add/Remove(owner, other, reason)`) used by both the link and the pushbox code.
- Minions keep today's blocking and their own steering separation (v1 scope = actors with `UGSTN_CombatSimComponent`).
- Tumble contacts: fighters stop being "faces" for `GSTN_TumbleContact` (only world geometry bounces a tumbling victim). Intended.

## 2. Pushbox data (same authoring path as the capsule, no BP wiring)
- New timeline track `FGSTN_TimelineTrack_Pushbox` next to Body Capsule: `Radius`, `HalfHeight`, `Offset` (same frame as the capsule:
  from the mesh root), `bEnabled` (a frame range with the pushbox off: cross-ups, dash-through). Vertical cylinder: the push is horizontal only,
  so nobody slides up a rounded cap again.
- Resolution per frame, first match wins: Pushbox track of the move > Body Capsule track of the move (same shape, radius x0.8) >
  reaction state entry (section 4) > standing pawn shape (radius x0.8). So dropkick/dair get a pushbox with zero new data.
- Authored by the same scripts (`Tools/GAS/setup_dropkick_body_capsule.py` and the dair one) and later by the hitbox editor (draw it like the
  capsule). Tuning per pawn on its existing combat data asset (where the hurtbox set lives), never in project settings:
  `PushGainPerFrame` (fraction of overlap removed per frame, default 0.25), `MaxPushPerFrame` (default 3 cm = 187 cm/s at 62.5 Hz).

## 3. Soft push inside the 62.5 Hz sim, rollback-safe
- Every entry of `FGSTN_StepSnapshot` gets the resolved world pushbox (cylinder centre, radius, half height, enabled). The snapshot is
  built once per frame from end-of-previous-frame state and rebuilt on rollback (`InvalidateSnapshot`), so all fighters read the same data.
- Each fighter's combat tick (it runs before Mover) computes ITS OWN share: for every other entry with overlapping Z ranges, horizontal
  penetration p = rA + rB - dist; push = min(Gain*p/2, Cap/2) along the horizontal line between centres (coincident: away along own facing,
  tie by EntityId). Sum, clamp to Cap. Symmetric by construction: A's and B's shares are equal and opposite, no messaging.
- Stored as `PushVelocity` in the combat sync state; a permanent additive layered move (pattern of `GSTN_LayeredMove_MomentumSlide` reading
  `ReadRunningSlide`) adds it to Mover's move. Mover still sweeps the ECB against the world: a cornered fighter does not move, its partner keeps
  overlap and keeps being pushed, so the free one is pushed out (Smash corner behaviour) with no special case.
- Frozen while either side is in hitstop (`bInHitstop` already in the snapshot). Applied in air and on the ground (decision 1).
- Perf: snapshot already exists and has a spatial grid; per fighter O(neighbours) with 2 float compares each, no physics queries added.

## 4. Fighters with no move timeline (knockdown, tumble, splat, crouch)
- New data asset `UGSTN_BodyShapeSet` on the pawn's combat data: map state tag (`Combat.State.Knockdown`, `.Tumble`, `.Splat`, later
  crouch) -> Body Capsule shape + anchor + Pushbox shape/off. The modifier and the push read the state from the sim state (same tags the
  ReactionAnimSet uses, never named in code). Order: move track > state entry > standing.
- Defaults: lying knockdown = low flat ECB (hh ~25), pushbox per decision 2; tumble = standing ECB, pushbox off; crouch idle/crouch attacks
  = their Body Capsule + pushbox tracks via the same scripts, done in the crouch checkpoint.

## 5. Tests (automation, timed script; numbers to logs per rule 11)
- Core: resolve order; push zero without overlap, away with overlap, capped, A+B = 0, coincident tie-break, disabled track, hitstop freeze.
- NP: two Mover pawns spawned overlapping separate within N frames, no frame moves more than Cap; A falls onto B and lands on the floor (ECB
  passes); cornered victim -> attacker pushed out; link start/end keeps both ignores; resimulating 10 frames gives identical positions.
- Capture (desktop): dair landing on the dummy and dropkick, per-frame attacker horizontal delta <= 3 cm, no 70 cm slide; rule 10 review.

## 6. Checkpoints and cost
- P1 desktop, Opus high: sections 1-3 + 5 core/NP tests, build, tests. 10-13 $.
- P2 desktop, Opus medium: section 4 + dair/dropkick data + capture + cloud rule-10 review. 7-9 $. Crouch attacks later, own brief.

## Owner decisions (reply e.g. "1A 2A 3A")
1. Push in the air? A) yes, same cap as ground (Smash Ultimate-like) [rec]. B) ground only, air fighters pass through. Cost same.
2. Lying knockdown victim: A) pushbox off, the attacker can stand over/cross a downed opponent [rec]. B) small pushbox on, a downed body still
   shoves. Data only, both kept as a per-state setting; cost same.
3. Scope now: A) P1 then P2 (~17-22 $) [rec]. B) P1 only, reactions stay on the standing shapes for now (~10-13 $; dair landing fixed,
   knockdowns not). Blocked meanwhile: dropkick director+agy pass waits for P1 either way.
