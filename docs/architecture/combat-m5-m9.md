# Combat M5–M9 — Editor, defence and links, piggyback, minions, stock GAS (outline specifications)

Part of the combat design ([combat.md](combat.md)). These milestones depend on results of M1–M4 (spike outcomes,
performance numbers, designer feedback), so they are specified at the level of goals, components, decisions and open
questions. **Before starting one, expand it to the level of the M1–M4 specifications** (files, types, algorithms,
tests, acceptance) in its own file, reusing this outline.

> **Revision 2026-10-19 (combat abilities):** "move" in this file means a **timeline ability**
> (`UGSTN_GA_BaseCombat` and its timeline, combat.md §11); type names follow M3.

## M5 — Editor tooling (`GSTNCombatEditor`)

**Goal:** designers create moves visually, without editing frame numbers in a details panel.

- **Timeline editor** (`FAssetEditorToolkit` for `UGSTN_CombatTimeline`; the same widgets edit the inline timeline of a
  `UGSTN_GA_BaseCombat` Blueprint):
  - Preview scene with the kit's preview mesh (skin chosen in the toolbar), the move's montage evaluated at the
    selected frame, hurtboxes and hitboxes drawn.
  - **Frame timeline**: ruler in frames (and ms), playhead with frame stepping (`,` `.`), one row per track, windows as
    draggable bars, keys as diamonds; phases (startup / active / recovery / FAF) derived and shown.
  - Hitbox **gizmos** (translate/rotate/scale on the selected key); add key at playhead; copy keys across frames.
  - Details panel for the selected track/key (`FGSTN_HitData`, variants).
  - **Bake sockets** button: samples the montage at the NPP tick rate (read from Network Prediction settings), writes
    `BakedSockets` for every socket referenced by `BakedSocket` hitboxes; a dirty flag when the montage changes.
- **Hurtbox set editor**: generate from the Physics Asset (one capsule per body, editable), mirror left/right.
- **Validation UI**: slot conflicts (M1 validator) shown on slot sets, weapons, kits and loadout presets; move
  validation (frame ranges, montage length, overlapping windows, root motion + movement track without mix mode).
- **Debug**: input display widget (M1 notation with frame gaps, consumed presses greyed), Gameplay Debugger category
  (combat state, history, active windows, last hits), Rewind Debugger / NP Insights trace of the combat state.
- *Open:* Sequencer-based timeline (reuse of `UMovieScene` tracks) vs a custom Slate timeline. Recommendation: custom
  Slate — the data is frame-indexed structs, not keyframed properties, and Sequencer's binding model adds friction.
- Acceptance outline: a designer builds a three-hit jab string with a baked-socket hitbox, cancel windows and an EX
  version only in the editor; validation flags a deliberately conflicting slot.

## M6 — Defence and links (shield, parry, tech, SDI, link system, grabs)

**Goal:** the defensive half of the platform-fighter ruleset, adapted to 3D and PvE.

- **Shield/block**: a kit move bound to the shield slot; while held: `Combat.State.Shielding`, `ShieldHealth` attribute
  (drains while held and on block, regenerates when released), **directional on the movement plane** (block arc
  around facing, configurable; hits outside the arc go through). Pipeline step `Shield`: blocked hits apply
  `ShieldDamage`, shieldstun (frames, state `ShieldStun`), no knockback; shield break → long stun state.
- **Parry / perfect shield**: window of N frames after the shield press (or a dedicated slot); pipeline step `Parry`:
  hit nullified, attacker gets a parry stun/punish window (attacker side is applied on the attacker's machine at its
  next tick through its own inbox — the same delivery mechanism in reverse).
- **Tech**: on contact with ground or wall while in hitstun/tumble, a tech press within `TechWindowFrames` before
  contact (history lookup) → tech in place / roll (moves); anti-mash lockout after a tech press (`TechLockoutFrames`)
  without tech. Wall tech uses the project's wall detection (`UGSTN_WallQueryUtils`), read through the tracker.
- **SDI** tuning and **DI** curves per game mode (rules asset).
- **Link system** (combat.md §12b), built here because grabs need it and M7 reuses it:
  - Start with a **link spike**: two players, one links to the other through the server, the linked one parks its
    simulated transform and follows the partner's socket (the parked-transform approach of §12b); measure corrections
    on both clients while the partner moves, with forced reconciles. Compare with the alternative where the follower's
    Mover follows the partner's socket transform as seen locally (expect constant corrections). Record in combat.md §17.
  - `FGSTN_LinkState` in the combat sync state, link requests and accept/refuse through the inbox, role tag projection,
    parked movement mode (`UGSTN_LinkedMovementMode`, shared by grabbed and rider), exit placement by the server,
    link breaking rules (hits, distance, death, timeouts), debug draw of links.
- **Grabs**: `Grab` hit type creates a grabber/grabbed link; throw slots in the grabber role; pummel; grab break by
  mash (the `Mash` condition); grab immunity frames after release.
- *Open:* shield and parry inputs in 3D PvE (always-available block vs limited shield); how supers interact with
  shields (unblockable flag per super).

## M7 — Piggyback and pair moves

**Goal:** a player rides a teammate; carrier and rider each have their own role-only actions; pair moves combine both.

- **Mount link** on the M6 link system: mount move with an `Interaction.Mount` volume (allies only), carrier-side
  `LinkRequest` resolution step (kit permission, not linked, not in hitstun, `Weight` ≤ `MaxCarryWeight`), predicted on
  the rider's side, server-confirmed.
- **Rider**: parked transform, presentation attached to the carrier's mount socket (per kit: `MountSocket`, offset),
  rider-only bindings (requirement `Combat.Link.Role.Rider` on the usual commands): its own attacks aimed with its own stick, dismount, pair
  move triggers. Hurtboxes and hitbox `Root` space from the carrier's socket transform.
- **Carrier**: normal movement and carrier-only bindings (requirement `Combat.Link.Role.Carrier`): drop, throw rider,
  pair move triggers; kit flags for moves disallowed while carrying; `GE_CarryLoad` scaled by the rider's weight.
- **Pair moves**: `UGSTN_PairMoveDefinition` (initiator role, both moves, start offset, link outcome); partner move
  started through the link inbox; at least two reference pair moves: carrier throws the rider (rider launched as a
  projectile-like move with its own hitboxes), rider boost-jumps off the carrier.
- **Hits while mounted**: rules for shielding angles and link breaking per role; meter and damage attribution for
  pair moves (both players credited, configurable).
- **Animation**: carrier and rider anim layers for the mounted state (kit-provided), IK hands/feet optional.
- *Open:* mount/dismount inputs; whether two riders or chains are allowed (default no); whether enemies can be mounted
  (would be a different link type).
- Acceptance outline: two clients with 60/15 ms and 2 % loss mount, move, attack from the rider, throw the rider,
  boost jump, dismount; corrections only at link changes and partner-side pair-move starts; harness snapshots equal.

## M8 — Minions (40+ enemies)

**Goal:** dozens of simple enemies with the same moves, hitboxes and knockback, at a fraction of the cost of a fighter.

- `UGSTN_MinionCombatComponent` (server only, no NP model), `UGSTN_MinionCombatSubsystem` ticking all minions in batch
  at the fixed tick, after fighters' combat ticks (so minions see the same snapshot), using Core for moves, hit
  queries and knockback; minion movement through Mover in a non-predicted configuration or a lightweight movement
  path (*Open*, to measure).
- **LOD**: by distance to the nearest player and activity (idle minions skip queries; far ones tick every N frames —
  their timers are frame stamps, so skipping is safe).
- **Replication**: FastArray of compact minion states (position, yaw, action state, current move + frame, hit
  flash flag), reduced frequency, client interpolation; hit presentation for player hits.
- **Interaction**: player → minion hits resolved on the server (lag compensation on minions); minion → player hits
  delivered to the player's inbox on the server.
- Budget to define with a profiling spike: target N minions at 62.5 Hz on the dedicated server, network bytes per
  minion per second.
- Promotion to fighter (elite/boss) = swap to the full combat component on spawn.

## M9 — Stock GAS adapter (optional)

**Goal:** use Core + GAS modules in a project with stock GAS (no Network Prediction).

- `GSTNCombatStockGAS` replacing `GSTNCombatNP`: combat state on a component simulated per frame on the owning client
  and the server, with GAS prediction keys for activations, server-authoritative hits, client-side input history sent
  with ability activation (`ServerSetReplicatedTargetData`-style payloads or a custom RPC with the last N events).
- No rollback: mispredictions are corrected GAS-style (ability rejection, effect rollback by prediction key).
- Decide after M4 whether there is demand; Core and GAS modules must not acquire NP dependencies in the meantime
  (checked by every milestone's acceptance criteria).

## Later — Held and thrown objects

See combat.md §13b. Expand into a milestone after M7 (a `Held` link on the M6 link system, plus projectile hitboxes).
