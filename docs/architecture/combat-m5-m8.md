# Combat M5–M8 — Editor, defence, minions, stock GAS (outline specifications)

Part of the combat design ([combat.md](combat.md)). These milestones depend on results of M1–M4 (spike outcomes,
performance numbers, designer feedback), so they are specified at the level of goals, components, decisions and open
questions. **Before starting one, expand it to the level of the M1–M4 specifications** (files, types, algorithms,
tests, acceptance) in its own file, reusing this outline.

## M5 — Editor tooling (`GSTNCombatEditor`)

**Goal:** designers create moves visually, without editing frame numbers in a details panel.

- **Move asset editor** (`FAssetEditorToolkit` for `UGSTN_MoveDefinition`):
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

## M6 — Defence (shield, parry, tech, SDI, grabs)

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
- **Grabs**: start with the spike from combat.md §12.7 (grabbed local player following an interpolated grabber).
  Candidate designs to compare in the spike: (a) server-authoritative grab with the victim's client fully corrected each
  frame while grabbed (simple, laggy); (b) the grabbed character's Mover follows the grabber's predicted socket
  transform sent through the victim's own correction stream (smoother, more complex); (c) during a grab the grabber's
  client also forward-predicts the victim as a local entity (requires ForwardPredict LOD for that pair — probably not
  supported by the ASC on proxies). Then implement `Grab` hit type, link state on both sides, "Grabbed" Mover mode,
  throw slots active only while grabbing, pummel, grab break by mash (the `Mash` condition).
- *Open:* shield and parry inputs in 3D PvE (always-available block vs limited shield); how supers interact with
  shields (unblockable flag per super).

## M7 — Minions (40+ enemies)

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

## M8 — Stock GAS adapter (optional)

**Goal:** use Core + GAS modules in a project with stock GAS (no Network Prediction).

- `GSTNCombatStockGAS` replacing `GSTNCombatNP`: combat state on a component simulated per frame on the owning client
  and the server, with GAS prediction keys for activations, server-authoritative hits, client-side input history sent
  with ability activation (`ServerSetReplicatedTargetData`-style payloads or a custom RPC with the last N events).
- No rollback: mispredictions are corrected GAS-style (ability rejection, effect rollback by prediction key).
- Decide after M4 whether there is demand; Core and GAS modules must not acquire NP dependencies in the meantime
  (checked by every milestone's acceptance criteria).

## Later — Held and thrown objects

See combat.md §13b. Expand into a milestone after M6 (it reuses grabs' attach logic and projectile hitboxes).
