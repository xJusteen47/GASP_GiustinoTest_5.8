# GSTNMinion

Minions for the GSTN combat: 40+ awake enemies on one host, simulated as data in a store instead of one actor each
([combat-m8-minions.md](../../docs/architecture/combat-m8-minions.md), [plan](../../docs/architecture/plans/m8-minion.md)).

Status: **experimental (0.1)**, `EnabledByDefault: false`. Built through M8 checkpoint **G12a**; the desktop items G6b and G9b are done, G7b is done apart from one open case (see Known limits), and G11b is open. Enabling it
changes nothing until a level spawns minions; nothing in the other plugins knows it (`Tools/Modules/check_modules.py` enforces it).

## Modules

| Module | Type | State | What it is |
|---|---|---|---|
| `GSTNMinionCore` | Runtime | **G2** | Pure: no world, no actors, no GAS, no Network Prediction. Tags, store and handles, intent, LOD logic, phase scheduler, attack tokens, net item codec, definition asset. |
| `GSTNMinionRuntime` | Runtime | **G3 to G12a** | Batch subsystem, pooled actors, presentation, spawner, brain subsystem, bridge seams. |
| `GSTNMinionEditor` | Editor | **G12a** | Definition and registry validators, wave validator (Data Validation). |
| `GSTNMinionTests` | Developer | **G12a** | Tests that need a world and harness helpers (`GSTNMinion.Runtime.*`). |

## What G2 contains (`GSTNMinionCore`)

- `GSTN_MinionTags.h`: native tags `Combat.Entity.Minion`, `Combat.Event.MinionSpawned/Despawned/Promoted`, `Minion.State.*` (the 12
  action states), `Minion.LOD.*` (the 4 tiers), `Minion.Type` (root only); `GetDeclaredTags()`. Defined in a Runtime module so a cooked game has them.
- `Store/GSTN_MinionStore.h`: `FGSTN_MinionStore` (struct of arrays, lowest-free-index allocation, 16-bit generation per slot,
  capacity from `gstn.Minion.Max`, default 160, clamped to the index range), `FGSTN_MinionHandle`, conversions to the pipeline id
  (`GSTNEntityKinds::MakeMinionEntityId`) and to `FGSTN_EntityRef{Minion, Index, Generation & 0xFF}`, `EGSTN_MinionActionState` (the 4-bit wire enum).
- `Intent/GSTN_MinionIntent.h`: what a brain asks (move, face, command, target); stale after 30 frames.
- `LOD/GSTN_MinionLOD.h`: `EGSTN_MinionLODTier`, `FGSTN_MinionLODParams` (borders, hysteresis 10 %, dwell 30 frames, intervals),
  `FGSTN_MinionLODMath` (pure tier decision), `UGSTN_MinionLODSettings` data asset.
- `Schedule/GSTN_MinionSchedule.h`: a slot of interval P runs when `(Frame + Index) % P == 0`.
- `Tokens/GSTN_MinionAttackTokens.h`: per-target cap (default 4), request, `Resolve()` grants by ascending slot index, release.
- `Net/GSTN_MinionNetCodec.h`: the 8.2 item layout (position 3 x 19 bits at 1 cm, yaw 8 bits, state 4, move id 6, move start 10,
  frame stamp 8, hit counter 3, health 5, speed 6, flags 3, optional type, global id and extension byte), `Serialize` to a bit
  archive, frame-stamp reconstruction for 8 and 10 bits. Quantize helpers live here until a second plugin needs them in `GSTNCore`.
- `Data/GSTN_MinionDefinition.h`: `UGSTN_MinionDefinition` (primary asset type `GSTN_MinionDefinition`) and its `IsDataValid`.

## Waves, corpses, promotion (G9a)

- **Corpses**: `UGSTN_MinionDefinition::CorpseSeconds` (default 2 s). When the Death timeline ends the slot goes `Dead` but is kept for
  that long (whole fixed frames, rounded up, stamped when the death ends): not moved, not brained, not hittable, no token, and the net
  item keeps publishing `Dead` so clients keep the body until the item is removed. 0 s frees on the frame the death ends. `Despawn` and the
  bench `Clear` still free at once. A type without a Death timeline still waits `gstn.Minion.CorpseFrames` first, then the corpse time.
- **Waves**: `UGSTN_MinionWaveDefinition` (entries: definition, count 1..64, spawn point actor tag, delay in frames, ring radius; at most
  128 in total) is played by a level-placed `AGSTN_MinionSpawnerActor`: `bStartOnBeginPlay`, a trigger box (a pawn enters, authority, once)
  or `StartWave()`. Every machine preloads the wave's assets asynchronously at begin play (`bPreloadOnClients`), the authority spawns by
  the batch frame, `GetAliveCount()` and `OnWaveCleared` (death, despawn and promotion all count; a dying minion is already gone).
- **Promotion (server core)**: `UGSTN_MinionSpawner::Promote(World, Handle, {FighterClassOverride, bKeepHealthFraction})` spawns the
  fighter class at the minion, adds a replicated `UGSTN_MinionPromotedComponent` (item id, pack id, type tag, health fraction, team),
  sets the team and frees the minion; null when refused (not alive, Spawning/Dying/Dead, no class, or `gstn.Minion.MaxFightersFromMinions`
  = 4 promoted fighters alive). The client swap and the wire flag are G9b (see Promotion presentation).
- Bench: `gstn.Minion.Bench.Wave <N> <CorpseSeconds=1>`, `gstn.Minion.Bench.Kill`. Tests: `GSTNMinion.Core.Wave.*`,
  `GSTNMinion.Runtime.{Corpse,Wave,Promote}.*`; harness `minion_wave`.

## Content without C++ (acceptance 3)

- **What a designer does**: right-click > Miscellaneous > Data Asset > `GSTN_MinionDefinition` (type tag, stats, hurtbox set, an existing
  combat timeline per move, reaction timelines, optionally a StateTree asset on `Brain`), then a `GSTN_MinionWaveDefinition` listing the
  definitions (count, spawn point tag, delay in frames), then a `GSTN_MinionSpawnerActor` in the level with that `Wave`. No C++, no registry.
- **The script**: `Tools/GAS/setup_minion_assets.py` (editor Python, idempotent, backs up to `Saved/Backups/GAS` first) makes
  `DA_Minion_Grunt`, `DA_Minion_Heavy` and `DA_MinionWave_Test` (6 grunts, then 2 heavies 125 frames later) in `/GSTNMinion/Test/`, with their
  timelines and hurtbox sets, and prints a 5-line summary. The test StateTree is transient (built by the editor module), so `Brain` stays
  empty and the script says so; assign a StateTree asset by hand.
- **Playing them**: `gstn.Minion.Bench.Definition <path>` (type of the bench) and `gstn.Minion.Bench.WaveAsset <path>` (wave played by
  `gstn.Minion.Bench.Wave`; N is then the asset's total).
- **Fallback**: an empty or unfound path logs a warning and uses the code-built definition or wave, so headless tests and the harness need no
  assets. `GSTNMinion.Runtime.Wave.TestAssetsSpawn` checks the assets validate and spawn through the spawner when they exist, and logs
  "assets not created yet" and passes when they do not.

## Promotion presentation and events (G9b)

- **Promoting body.** A promoted minion stays for `gstn.Minion.PromoteHoldFrames` (16 frames) as a Promoting body: the wire flag is set, it is Dead, its actor collision is off and it is on the corpse list. Then it is freed. A Promoting body never shatters.
- **Swap, not cross-fade.** The fighter (`UGSTN_MinionPromotedComponent::BeginPlay` -> `NotePromotedFighter`) is hidden behind the puppet for `gstn.Minion.Present.PromoteSwapSeconds` (0.1 s), then the puppet is pooled. A held puppet gives up after `PromoteHoldSeconds` (1.0 s). The puppet has no fade parameter, which is why it is a swap. Log: `MinionPromote swap|nopuppet|expired|lostpuppet`.
- **Events.** `Spawned` and `Despawned` are queued to the batch flush; `Promoted` is published inside `Promote`, on the authoritative stream (the `Event_Minion*` events).
- **LOD "no pop".** Dead reckoning is capped at the sender's own sample gap (`FarDeadReckoningCapMs` = 0), and senders slower than T1 hold, so far puppets trail and never lead. Spike 8 (dedicated + 2, 60/15): pop 2.1 cm, swap wait about 100 ms, first control 178-185 ms, the same at 0 % and 2 %.
- **Bench.** `gstn.Minion.Bench.Promote [class|-] [X Y Z]`.

## Hit presentation grace and confirm window

- **Grace.** The flicker of a predicted minion hit is Network Prediction correction of the victim's hit ring during a resimulation, not a miss. A predicted hit whose key vanishes is a cancel only after 0.25 s away; a key that comes back in that time is the same hit (no second prediction), and keys are forgotten after 2 s (`gstn.Minion.Present.LogFlash 1` logs `MinionFlash`). The pawn's `UGSTN_HitPresentationComponent` uses the same rule through the shared helper `FGSTN_HitPredictionGrace` (GSTNCombatNP; `CancelGraceSeconds` 0.25, `PredictedHitMemorySeconds` 2).
- **Confirm window.** A minion hit confirms within 0.5 s with no ping (host and tests). With a ping from the PlayerState the window is min(1.5, max(0.75, 4 x RTT + 0.25)) s. Confirms inside 0.5 s are unchanged. Pawns have no confirm window.

## Bridge seams (G10)

Generic hooks for optional bridge plugins (platforms: `GSTNPlatformMinion`, parts: `GSTNPartsMinion`; both come later and `GSTNMinion` depends on
neither). All are inert while nothing is bound: no publish byte, LOD decision or test changes, and no per-frame work beyond two integer tests.
- **Platform flag**: `UGSTN_MinionCombatSubsystem::SetPlatformActor(Handle, bool)` (store `ExtFlags`). A flagged minion has no pack item (an
  existing one leaves like a despawn's; clearing the flag adds it back); the store keeps simulating it. Reset when the slot is freed.
- **`IGSTN_MinionPublishObserver`**: `AddPublishObserver` / `RemovePublishObserver` (raw pointer, the observer unregisters itself). Called once
  per batch step, at the end of `StagePublish`, with the end-of-step pose and velocity of the flagged slots, in slot order.
- **LOD ceiling**: `SetMaxLODTier(Handle, tier)` (store `MaxLODTier`, default Dormant = none) holds a slot no farther than a tier, pulled back at
  once. `SetExemptFromCombatCap` plus `IsExemptFromCombatCap(Slot)` mark slots the G11 `MaxActiveCombat` demotion must skip.
- **Pose samples**: `SamplePose(Handle, Frame, Position, Yaw, bClamped)` reads the lag compensation history (nearest record, clamped outside the ring).
- **ExtByte**: `UGSTN_MinionDefinition::bHasNetExt` adds the 8-bit extension byte to the net item of that type (0 bits otherwise), built into the
  wire format on both sides from the definition; `SetExtByte(Handle, byte)` on the authority, `UGSTN_MinionClientRegistry::GetExtByte(GlobalId)` on a client.
- **Hit flow seams (G10b, for GSTNPartsMinion, M4b P16)**: `SetExtraVictimStep` (a step class run right before ComputeKnockback for minion victims),
  `VictimViewExtension` (single delegate, called at the end of every minion victim view: a bridge points the view's snapshot extension at its own
  data), `ApplyOutcomeExtensions` (single delegate, called once per applied hit before the death test with the resolved context: a bridge writes
  its own columns and returns true to kill through the normal death path, Health set to 0) and `OnOutcomeApplied` (multicast, after the store write).
  Unbound, each costs one test and changes nothing.
- **Net flag seams (for GSTNPartsMinion, M4b P16 B)**: `OnDefinitionRegistering` (multicast, server, in `RegisterDefinition` before the type
  table is published: the one moment a bridge may set `bHasNetExt`) and `UGSTN_MinionClientRegistry::OnDefinitionResolving` (multicast, client,
  before the codec table is read), so a client in its own process builds the same format from the same config. Unbound, nothing changes.
- Tests: `GSTNMinion.Core.Seams.*`, `GSTNMinion.Runtime.Seams.*`.

## Host budgets (G11)

Two cvars cap what the host pays when many minions fight (M8 4d.1). Applied in this order if the host frame does not fit:

1. `gstn.Minion.MaxActiveT0` (default 48, 0 = no cap): after the LOD step, the farthest T0 minions beyond the cap are demoted to T1. Never demoted: a minion
   that holds an attack token, one in a timeline or reaction (the forced-Combat test), one exempt through `SetExemptFromCombatCap`, one whose bridge ceiling is
   Combat. Ties break by slot (higher slot first). A demoted slot restarts its dwell and, when the stepper pulls it back, is sent back with no tier event.
   `GetActiveT0Count()` and `GetT0DemotedCount()` expose the last step.
2. `gstn.Minion.T0RateHz` (default 15, clamped 5..15): the T0 replication rate; 15 sends the same bytes as before. Core keeps no cvar, the Runtime hands the
   value to `GSTNMinionNetRate::SetCombatRateHz` in `NetPublish`.
3. Then (not in this plugin yet): the puppet cap `MaxVisiblePuppets` and puppet LOD (M8 8.4); record the max N honestly.

`gstn.Minion.Bench.Report` prints a parseable `MinionBench budget activeT0=.. demoted=.. awake=.. dormant=.. t0=.. t1=.. t2=.. t3=..` line;
`gstn.Minion.Bench.Population <N=150> <NearCount=60> <FarRadius=12000>` places the 150 population case. Harness: `scenario_minion_horde`,
`scenario_minion_bytes` (SUITE_MINION_BENCH). Numbers from a laptop with -nullrhi are provisional.

## How to extend

- **A new minion type, no C++.** Make a `UGSTN_MinionDefinition` asset: a type tag (a leaf under `Minion.Type`, add it to your project's tag ini), stats and capsule,
  a hurtbox set, up to 31 moves (a command tag, a timeline, a range window, a cooldown), the Hitstun and Death timelines, optionally the puppet class or mesh, a brain
  and a LOD override. `Tools/GAS/setup_minion_assets.py` writes a grunt and a heavy under `/GSTNMinion/Test` as a worked example. The registry is filled in
  first-spawn order and the clients check its hash, so nothing is listed anywhere: spawning the type registers it (at most 256 types).
- **A new wave.** A `UGSTN_MinionWaveDefinition` asset (entries: definition, count 1..64, spawn point actor tag, delay in frames; 128 in total) played by a level-placed
  `AGSTN_MinionSpawnerActor`. The editor validators check the total against `gstn.Minion.Max` (default 160) and that every definition path loads.
- **A brain.** A StateTree on the schema `UGSTN_MinionStateTreeSchema` (tasks MoveToTarget, FaceTarget, RequestAttack; conditions TargetInRange, IsLODAtLeast) set as the
  definition's `Brain`; a definition with none gets the native chase and swing brain. The tree writes an intent only; add a task by subclassing, never touch the store.
- **Bridge seams (G10).** Platforms and parts plug in from outside through `SetPlatformActor`, `IGSTN_MinionPublishObserver`, `SetMaxLODTier` and `bHasNetExt`
  (see above). GSTNMinion never depends on those plugins.
- **Host budgets (G11).** `gstn.Minion.MaxActiveT0` and `gstn.Minion.T0RateHz` (see Host budgets); measure with `gstn.Minion.Bench.Population` and `gstn.Minion.Bench.Report`.
- **Debug draw (G12, not in Shipping, host world only, nothing is drawn or ticked while all are 0).** `gstn.Minion.Debug.Capsules` (capsule per minion, colour by tier),
  `.Hitboxes` (the reach sphere of the move being played), `.LOD` (tier text), `.History <N>` (last N lag compensation poses as points), `.Intent` (intent arrow, brain waypoint line).
- **Validators (G12, editor).** `UGSTN_MinionDefinitionValidator` and `UGSTN_MinionWaveValidator` run under Data Validation (save, on demand, `-run=DataValidation`) over the
  rules in `GSTNMinionValidation::` (Core), which the `GSTNMinion.Core.G12.*` tests run headless.

## Gameplay tags

Native: see `GSTN_MinionTags.h`. Designer vocabulary (ini only) is in `Config/Tags/GSTNMinion.ini` (`Minion.Type.Grunt`,
`Minion.Type.Heavy`, `Command.Minion.Attack.Swing`). The plugin's Core module registers that folder with the tag manager at startup
(the engine reads `Config/Tags` for ordinary plugins only through this call), the same way `GSTNDefence` does. Add your own types in your project's tag ini.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`: entity ids, hurtbox set and timeline assets), `GSTNCombatNP`, `GSTNCore`, and `StateTree` (the brain).
- `GSTNMinionCore` lists engine modules, `GSTNCombatCore` and `GSTNCore` only; never GAS, NP, Mover, an Editor module or a sibling minion module.
- Fork patch: the post-fixed-tick hook is a GAS-NPP fork change, `OnPostFixedTick(ServerFrame, EndTimeMS, bResimulating)`. It is inert while nothing is bound; `PostToInbox` takes a generation and `FindEntity` returns `FGSTN_FoundEntity`.
- Nothing requires `GSTNMinion`. No `/Game/` path and no `ConstructorHelpers` anywhere in `Source`.

## Behaviour notes

- **Batch (G3).** The batch runs its stages in order: 0 spawn queue (`gstn.Minion.SpawnsPerFrame`, default 4), 1 LOD, 2 drain, 3 timeline, 4 movement, 5 publish. Actors come from a pool (`AGSTN_MinionActor`). The subsystem activates only when NetMode is not Client.
- **Net item (G2, G6a).** The minimal item is 119 bits (the spec estimated about 121). The item type is sent on every update (9 bits), not only on add, because a type on add needs a per-connection baseline. Generation 0 is the null handle; generations wrap from 65535 to 1.
- **Attack tokens.** "Token holder" is approximated as "a member running a move".
- **Hit flow (X4).** A minion victim is rewound from the world subsystem's history ring through the provider snapshot; the fighters use the same ring, there is no second one.
- **Brain (G8).** Each brained slot runs one `FStateTreeExecutionContext`, driven by `UGSTN_MinionBrainSubsystem`; there is no `UStateTreeComponent` per actor. The brain's tier rates are cvars, not the LOD batch intervals.
- **Promotion (G9a).** The promoted fighter gets no GAS attribute write: its health fraction sits on `UGSTN_MinionPromotedComponent` and the kit applies it (G9b). The Promoted event is published on the authoritative stream from G9b (see Promotion presentation).
- **Host cap (G11a).** `gstn.Minion.MaxActiveT0` (default 48) replaces the per-definition `MaxActiveCombat`, which nothing reads now.
- **Store (G12a).** The default capacity is 160; a single wave stays capped at 128. There is no registry asset: the type table is filled in first-spawn order and checked by hash, so the registry rule is a function run over a wave's types.

## Known limits

- **Movement.** Spike 3 picked backend B (lightweight sweep and slide). No Mover backend is built, and the course is built in code, not as `L_MinionCourse`. The overhang traps G4 saw at the stairs (in fact next to a cylinder) are fixed (2026-11-08, test `OverhangStepDownNoPenetration`); a 10-minute soak showed no penetration trap, but one Launched stall (a minion hovering about 1 cm above the floor) remains, cause not found.
- **Rewind cost.** A cheap candidate query exists but is not wired into GSTNCombatNP, so the rewind cost still grows with N. Meter gain on minion victims is skipped.
- **Net rate.** `MaxRelevantPerClient` is a cvar. The push-model ini and the bake-off (G6b) are not done.
- **Presentation (G7a).** The Animation Budget Allocator is skipped (plugin not enabled). `UGSTN_HitPresentationComponent` cannot present minion victims, so the attacker's hit pause and local cue do not play on minion hits; that needs a GSTNCombatNP hook, which X4 forbids. The player-to-minion bench scenarios need `gstn.Minion.Bench.HurtboxSet` and a swinger kit. Far and Dormant client puppets no longer overshoot (G9b: dead reckoning capped, see Promotion presentation).
- **Host budget.** A demoted T0 slot is pulled back by the stepper: perf debt, not measured.
- **Net format (G10).** `FGSTN_MinionNetFormat::ExtTypes` is filled process-wide, so a pack decoded before the registry fills it uses the wrong table. Open for P16 (see GSTNPartsMinion).
- **Desktop items open:** G7b (one case, see Status) and G11b; the horde and bytes numbers are laptop numbers (`-nullrhi`), provisional.

## Tests

`GSTNMinion.Core.*`, no world: `Store.*`, `Schedule.*`, `Tokens.*`, `LOD.*`, `Net.*`, `Tags.*`, `Definition.*`, `Wave.*`. Run headless:
`UnrealEditor-Cmd GASP_Giustino2.uproject -ExecCmds="Automation RunTests GSTNMinion.Core; Quit" -abslog=<file>` and read the counts from the log.
`GSTNMinion.Core.G12.*` (capacity default 160, registry and wave cross-asset rules), `GSTNMinion.Runtime.G12.*` (debug draw off does no work).
