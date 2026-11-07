# Handover M8 minion

## G0a (laptop, 2026-11-06): DONE, gate green

- Start: local feature 3312333 (SD-16 + 5 SD commits, unpushed; M7 G7-G12/M7-15 not merged, step 0 not done). Branch `claude/project-thread-xhi73b`.
- Pre-gate (SD-16's skipped one): headless on 980/0, plugin-off build + suites 933/0, rebuild on OK (`Saved/SD17/gate.txt`). Recorded in spin-drill.md.
- S0: X1..X14 re-checked, none worse than written. X2 has no kind field: the kind rides the EntityId bit of X5, as planned.
- Spike 2: hook point found (after the lag-comp capture in the fixed-tick loop, and after `StepRollback`); no delegate exists, so G1 applies
  the inert `OnPostFixedTick(ServerFrame, EndTimeMS, bResimulating)` patch (text in the report). Time map: frame F ends at `F*FixedStepMS`;
  after the tick the authority "now" is 1 frame behind the newest history stamp. Measured error 0 frames (round trip 0.47). Report:
  `docs/notes/m8-spike-report.md`, combat.md §17e.
- Side finding for FP: `GetSyncStateAtTime` ignores `Offset` (fine on server, wrong by Offset frames on a client).
- Leftover: scratch test `Source/GASP_Giustino2/Private/ScratchSpike/GSTN_SpikeFixedTickTest.cpp` is untracked and must be deleted by hand
  (delete was blocked by permissions); the next build drops it from the binaries.
- Not done (per owner): nothing beyond G0a. **G1 waits for G0b** (desktop, spike 1 baseline on a low-end budget, needs L_Test_Hub).
- Cost: one Opus thread, ~45 tool calls, 2 gate builds + 1 scratch build (~1 h machine), ~$5.

## G0b (laptop, 2026-11-07): DONE as a laptop measurement; G1 NOT started (cost cap)

- Branch `claude/project-thread-q54iwa` = a97ef43 (wlggpb) + merge of PR #26 head f7d413b (M8 plan, doc only; its merge commit is
  mislabelled "Merge M8-G0a") + merge of xhi73b 3c14906 (G0a + SD-13..16). Both merges clean.
- Spike 1 run on the laptop (coordinator's assignment), rendering editor PIE, L_Test_Hub, host + 2 clients: host ~0.5 ms per idle AI
  fighter per fixed step (10x over target), frame > 16 ms already at N=10. Numbers, limits and proposed low-end budgets in
  `docs/notes/m8-spike-report.md` (G0b). M8 goes ahead with the minion store; the desktop repeat is optional.
- Leftovers to delete by hand (deletes are denied): `Source/GASP_Giustino2/Private/ScratchSpike/GSTN_SpikeBrawl.cpp` (untracked; the next
  build after deleting drops `gstn.Spike.Brawl`), scripts in `Saved/M8G0/`.
- Cost: ~95k context, 3 builds + 3 editor runs (~25 min machine), ~$6-8.

**Brief M8-G1 (laptop, Opus medium, estimate $8-12: M size + four suites at 0/2 % on the laptop):**
1. Start from `claude/project-thread-q54iwa`; read plan §0, §3 G1, this file and the spike report (G0a hook patch text).
2. Separate commits: (a) `GSTNEntityKinds` minion id + `FindEntity` dispatch on `MinionEntityBase = 0x40000000`, generation check, stub
   provider test; (b) `FGSTN_HitSyncState::MaxHitTargets` 8→16 (`GSTN_CombatHitState.h:83`, fix test "the ring holds eight", 12-target
   cleave test); (c) fork patch `OnPostFixedTick(ServerFrame, EndTimeMS, bResimulating)` in `NetworkPredictionWorldManager` (two broadcast
   points, `PATCHES.md` row; tell the owner before it lands); (d) `FGSTN_LagCompTime` (`TimeMS = Frame*FixedStepMS`) + round-trip test;
   (e) `Combat.Entity.Fighter` native tag on HitLanded/HitReceived/KO events.
3. Gate: full build, headless `Automation RunTests GSTN` = baseline (measure it first on this HEAD) + new tests; SUITE_DEFAULT, PARTS,
   DEFENCE, LINK at 0 % and 2 %, unexplained reconciles 0 (the sync-state growth must not add any). Harness: Saved/R31 driver pattern.

## G2 part "GSTNCore" (desktop, 2026-11-07): DONE; the rest of G2 is NOT done

- Branch `claude/project-thread-k72g89` from `claude/project-thread-q54iwa` (2a0c80e). Independent of G1 (touches no combat or fork file).
- Done: new plugin `Plugins/GSTNCore` (module `GSTNCore`, Runtime, Core only, `EnabledByDefault: true`, README, index row);
  `FGSTN_PoseHistory`/`FGSTN_PoseSample` moved (git mv) to `GSTNCore/Public/Net/GSTN_PoseHistory.h`, API `GSTNCORE_API`;
  `GSTNPlatformCore/Public/GSTN_PoseHistory.h` is now a redirect header; `GSTNPlatform.uplugin`, `GSTNPlatformCore` and
  `GSTNPlatformRuntime` Build.cs list `GSTNCore` (Runtime needs it explicitly: transitive public deps did not link, LNK2019).
  `check_modules.py`: GSTNCore may list no GSTN, GAS, NP, Mover, Editor or project module. New test
  `GSTNCore.Net.PoseHistory.ResimOverwritesAndReset`.
- Not moved: there is no quantizer in GSTNPlatform to move (the plan's "and the quantizer" is new code, G2 Core, goes in `GSTNCore/Net`
  when written). Swept bounds/payload column of X9 not added (G2/G5 when the minion side needs them).
- Gate: editor build OK (`Saved/M8G2Core/build2.txt`), headless `GSTNCore+GSTNPlatform` 23/23 green, the two
  `GSTNPlatform.Core.PoseHistory.*` unchanged and green. Full GSTN suite and plugin-off build not run (no combat/NP code touched).
  `check_modules.py` reports one old problem not mine: the committed .uproject on q54iwa lists GSTNPiggyback.
- Next for G2 (laptop, after G1): plugin `GSTNMinion` skeleton + Core store/LOD/tokens/codec; skip the pose history move.
- Cost: ~70k context, 2 builds + 1 test run (~5 min machine), ~$4-5.

## G1 (laptop, 2026-11-07): DONE, gate green except a pre-existing SUITE_DEFAULT flake

- Branch `claude/project-thread-qeaif9` from q54iwa (2a0c80e). Commits a291d30 (a), 397d9f3 (b), 7fed994 (c), 33356bb (d), 53ea28b (e);
  then merge 3e5e503 of k72g89 (GSTNCore). Owner told of the fork patch before it landed (thread, 02:3xZ).
- (a) `GSTNEntityKinds::MinionEntityBase` + `MakeMinionEntityId/MinionIndexOf/KindOfEntityId`; `FindEntity` returns `FGSTN_FoundEntity`
  (registry source or the provider whose `GetEntityKind()==Minion`); `FGSTN_SnapshotEntry::Generation`; `PostToInbox(Id, Generation, Entry)`
  calls the new `IGSTN_HittableProvider::PostToEntityInbox` (the 2-arg form takes the generation from the held snapshot). Inert with no provider.
- (b) `MaxHitTargets` 16; wire sends Count-1 in the same 4 bits (no size change); `MaxTargetsPerGroup` ClampMax 16, default still 8.
- (c) `UNetworkPredictionWorldManager::OnPostFixedTick(ServerFrame, EndTimeMS, bResimulating)`, two broadcasts, PATCHES.md row.
- (d) `FGSTN_LagCompTime::TimeOfFrame/FrameOfTime` in `GSTN_LagCompensation.h`. (e) native `Combat.Entity.Fighter` on HitLanded/HitReceived/KO.
- Tests: headless GSTN 1302 -> 1306/0, 0 ensures (G1); 1307/0 after the GSTNCore merge. New: `GSTNCombat.NP.EntityKind.{IdRange,Dispatch}`,
  `...Attacker.CrowdCleaveTwelve`, `...LagComp.TimeRoundTrip`; ring/overflow tests moved to 16.
- Harness (listen + 2, 60/15 ms, `Saved/M8G1/driver_suites.py`, reports `Saved/Logs/m8-g1/`): PARTS 10/10, DEFENCE 13/13, LINK 19/19
  at 0 % and 2 %. DEFAULT must run on `L_Test_Combat` (first overnight run used DefaultLevel: void for hit/trampoline). On L_Test_Combat
  the rest passes; hit_basic, di, dodge_blocked_in_hitstun, trampoline, hit_host (+ flips of stagger_threshold, friendly_fire,
  tilted_gravity_jump) fail on G1 AND on the pre-G1 baseline 2a0c80e built the same way: a stray hit on pawn 3, group 3, ~frame 400
  (`(392, 3, 3, 0, 1, 0, 6)`). Pre-existing on this laptop state, not caused by G1; worth its own look (open).
- Not done: dedicated-server runs, plugin-off build (nothing in G1 is optional), docs (combat.md X5-X11 rows) beyond PATCHES.md.
- Cost: G1 ~ $9-11 (one Opus thread; 4 builds, ~4 h machine harness incl. the baseline).

## G2 (laptop, 2026-11-07): DONE, gate green

- Commit after f61b253 on `claude/project-thread-qeaif9`; includes the desktop GSTNCore part (merge 3e5e503). Code written by a Sonnet
  subagent, two compile/test fixes by me (`QuietNaN` -> `std::numeric_limits`; a test that added an array element to its own array).
- `Plugins/GSTNMinion` (EnabledByDefault false, enabled in `.uproject`): Core module only has content; Runtime/Editor/Tests are shells.
  Choices to review in G3: health on the wire 5 bits as fraction x 31; minimal net item 119 bits (spec ~121); generations start at 1 and wrap
  65535 -> 1 (0 = null handle); LOD dwell bypassed only by `bForceCombat`; tokens never preempt, granted by ascending slot; the codec has its
  own `GSTNMinionQuant` helpers (no quantizer existed in GSTNPlatform or GSTNCore); `KnockbackModel`, StateTree and the "supported tracks"
  check left out of the definition (runtime classes, G3+); `gstn.Minion.LOD.*` cvars not added.
- Gate: build OK; headless GSTN 1322/0, 0 ensures (= 1307 with the plugin absent + 15 `GSTNMinion.Core.*`); `check_modules.py
  --allow-uproject` 0 problems (without the flag: the old GSTNPiggyback .uproject note). Harness not needed (no NP or fighter code).
- Not done: G3 (next, L size). Docs (combat.md, plugin index row for GSTNMinion) not updated.
- Cost: G2 ~ $5-7 (Sonnet writer ~230k tokens, 3 builds). Thread total G1+G2 ~ $16-19.

## G3 (laptop, 2026-11-07): DONE, gate green (harness suites not run)

- Branch `claude/project-thread-pk0j51` from qeaif9 (bf8562c); commit c1bf627. Code by an Opus writer subagent from
  `Saved/M8G3/writer-brief.md`; its report (files, choices, every stub) is `Saved/M8G3/writer-report.md` (not versioned).
- Runtime: `UGSTN_MinionCombatSubsystem` (game/PIE worlds, activates in OnWorldBeginPlay when NetMode != Client; provider + OnPostFixedTick
  bound; skips resim and repeated frames), stages 0 spawn queue (`gstn.Minion.SpawnsPerFrame` 4) / 1 LOD / 2 drain stub (flat damage only)
  / 3 timeline with skip-once events / 4 flat kinematic placeholder / 5 publish (actor transform T0/T1, stats). `AGSTN_MinionActor` pooled,
  `UGSTN_MinionCombatComponent` (SetIntent, RequestCommand, GetHealth, OnMinionHit/Died/LODChanged, sent after stage 5),
  `UGSTN_MinionSpawner` (slot claimed at once, activated by the queue), bench subsystem + `gstn.Minion.Bench.{Spawn,Clear,Report}`,
  `AutoReportSeconds`/`AutoQuit` cvars for timed headless runs, `stat GSTNMinion`, Insights scope `GSTN_MinionBatch`.
- Choices to review: intent/handle are BlueprintType (MoveDir now FVector2D); reaction MoveIds 62 (spawn) / 63 (death) for G6; hits wake a
  slot (tier forced to Combat); Dying minions left out of the snapshot; test hooks are not under WITH_DEV_AUTOMATION_TESTS (UHT accepted);
  `GSTNMinion.uplugin` now lists NetworkPrediction.
- Stubs: G5 rules pipeline, tokens, attack queries, hitstop; G4 movement backend; G5/G6 history ring and packs; G9 spawn events.
- Gate: build on OK; headless GSTN 1326/0, 0 ensures (1322 + 4 `GSTNMinion.Runtime.*`); plugin off build OK, 1307/0 (= G1 baseline);
  grep gate clean. Bench headless (`-game -nullrhi`, DefaultLevel, `Saved/M8G3/bench.ps1`): N=40 Idle 3750 frames avg 19.3 us max 566 us;
  Chase avg 13.2 us max 260 us; 0 ensures. Chase movement not checked by eye (nullrhi).
- Not done: `Tools/GAS/setup_minion_assets.py` written but NOT run (needs the editor; asset property names untested), so no
  DA_Minion_Grunt/Heavy or L_MinionYard yet. Harness SUITE_* not run: G3 touches no fighter or NP code, the provider is empty with no minions.
  Docs (combat.md, plugin index row) not updated.
- Pitfall: `-ExecCmds` separates commands with commas, not semicolons (only the first ran).
- Cost: G3 ~ $10-13 (Opus writer ~245k tokens, my context ~75k; 4 builds, 2 headless runs, 2 bench runs, ~1 h machine).

## G4 (laptop, 2026-11-07): code DONE, spike 3 decided (B), robustness NOT at 0 failures (2 fix attempts used)

- Branch `claude/project-thread-pk0j51`, commit 34c4e96 after G3. Opus writer from `Saved/M8G3/writer-brief-g4.md`; report with
  choices, stubs and doubts: `Saved/M8G3/writer-report-g4.md` (not versioned).
- `IGSTN_MinionMovementBackend::StepBatch`, backend B `FGSTN_MinionMovement_Lightweight` (sweep + slide, step up 45 cm, ~45° walkable,
  ground snap, ledges, Falling/Launched with the gravity vector at the position, PushedBack grounded, depenetration one shape per pass,
  separation by spatial hash in slot order, KillZ). Subsystem: `LaunchMinion`, `SetGravitySource`, `OnMovementStepped`. Definition
  `StepHeight` 35 -> 45. No `GSTN_MinionBlock` channel (would need project ini): WorldStatic+WorldDynamic object query.
- Bench: `Massacre`, `gstn.Minion.Bench.Course 1` (course from /Engine cubes: stairs, 40° ramp, 10 cm wall, corner, 300 cm ledge),
  spawns dropped onto geometry, counters fell/trapped(stuck=)/blocked/tunnelled. Chase walks to the bench centre without a player
  (not comparable with G3 Chase numbers).
- Tests: `GSTNMinion.Runtime.Movement.*` 10 (incl. FallingAgainstWallLands). GSTNMinion 29/0; full GSTN 1335/0, 0 ensures measured before
  the two fixes (minion-only changes after; expected 1336). Plugin-off: no file outside GSTNMinion changed, so = G3's 1307/0 (not rebuilt).
- Spike 3 (headless `-game -nullrhi`, laptop, not the desktop render host): cB = 0.0056 ms (Chase, 60 s) / 0.0065 ms (Massacre, 10 min)
  per minion-frame. A not built. Decision rule: cost passes rule 2, robustness does not -> rule 3: ship B, record honestly.
  Massacre 10 min N=60: fell 0, tunnelled 0, trapped 5, blocked 18, 0 ensures. Chase 60 s: trapped 1 (Falling hover at
  (-1014,664,91), unchanged by fix 2), blocked 7.
- OPEN (next movement thread): trapped "in penetration after its step" at the stair block side face (~(1030,430,91), stairs are Y ±200),
  and the Falling hover above. Log lines in `Saved/M8G3/bench-*.log`. Suspect: side-face contact of a capsule against box edges in the
  step-up/snap path. Not fixed (2 attempts used).
- Not done: L_MinionCourse as an asset (course is built in code instead), Mover backend A, docs.
- Cost: G4 ~ $9-12 (Opus writer ~290k tokens over 3 rounds, my context ~95k; 4 builds, 2 test runs, 3 bench runs incl. the 10-min soak).

## G5 (laptop, 2026-11-07): code DONE, gate green; harness scenarios and spike 5 numbers NOT done

- Branch `claude/m8-g5` from `claude/project-thread-pk0j51` (04baf07). Opus writer subagent from `Saved/M8G5/writer-brief.md`; report with
  files, choices and doubts: `Saved/M8G5/writer-report.md` (not versioned). Nothing outside `Plugins/GSTNMinion` changed
  (uplugin and Runtime Build.cs now list GSTNCombatGAS and GSTNInput; tests Build.cs lists AIModule).
- Done: stage 2.2 drain through the rules pipeline on a victim view (MeterGain skipped; Hitstop/Hitstun/PushedBack/Launched by tier, below
  threshold only HitCounter), last-attacker ring, HitBy dedupe, KO event; provider snapshot minion fields; `HurtboxIndex` in PendingHits,
  optional pipeline step slot, `OnOutcomeApplied` hook (inert, for M4b P16); stage 2.3 attack queries against fighters, hitstop pausing,
  stamped delivery to the fighter inbox; attack tokens (`gstn.Minion.MaxAttackersPerPlayer`, default 4) released on death/hit/timeline end;
  bench commands `gstn.Minion.Bench.Cleave` and `gstn.Minion.Bench.LagComp`.
- X4 decision: minions use the world subsystem's history ring through the provider snapshot (no separate FGSTN_MinionHistory needed for
  the fighters' rewind). The cheap candidate query `GatherRewoundCandidates` (grid inflated by MaxMinionSpeed*RewindMs + swept bound) exists
  but is NOT wired into the fighters' rewind: that needs a GSTNCombatNP change, so rewind cost still grows with N. Open for G6/G7.
- Dedupe key is (attacker, hit frame, hitbox index): the hit entry carries neither TimelineStartFrame nor HitGroup.
- Gate: editor build OK first try; headless GSTN 1343/0, 0 ensures (1336 + 7 `GSTNMinion.Runtime.Hits.*`). Plugin-off build not run (no
  file outside GSTNMinion changed).
- Not done: spike 5 measurement (commands exist, not run); harness `scenario_minion_hits_player`, `scenario_minion_tokens` at 0/2 %;
  timeline movement track, knockdown on landing, reaction timelines still stubs; the open G4 trapped-minion bug untouched; docs.
- Cost: G5 ~ $9-12 (Opus writer ~490k tokens, 158 tool calls; my context ~40k; 1 build, 1 test run).

## G5 closing (laptop, 2026-11-07): harness scenarios green at 0 % and 2 %, spike 5 recorded

- Branch `claude/project-thread-aj375g` (from `claude/m8-g5`). Added the bench `Attack` scenario (`gstn.Minion.Bench.Attack <N> <FighterIndex>`: attackers with a
  40 frame swing walk up to the Nth registered fighter and swing in a loop; Report prints `MinionBench attack started/tokensDenied/attackHits/maxHolders`), three
  bench totals in the subsystem, and renamed the test helper `TestCommand` -> `HitTestCommand` (unity build clash with RuntimeTests, build failed once).
- Harness (`Tools/GASNPPTest/minion_scenarios.py`, `SUITE_MINION`, dedicated server + 2 clients, lag 60/15, -nullrhi, driver in `Saved/M8G5`, not versioned):
  `minion_hits_player` 0 %: PASS 7 checks, 2 %: PASS 7. `minion_tokens` 0 %: PASS 7, 2 %: PASS 7. (First 0 % run of hits_player failed on my own check
  "no other pawn hit": the two pawns start close, one swing reaches both; replaced by "the pawn the bench named is hit". Not a code bug.)
  Tokens run at 0 %: started 48, tokensDenied 778, maxHolders 4 (cap 4 held and reached), 8 minions, 85 attack hits.
- Spike 5 (editor PIE dedicated, `-nullrhi`, laptop, 60 Chase minions, 40 cleaves of 12): drain 8.90 us per hit, cleave of 12 = 106.8 us; rewound candidate
  query 0.63 us per query (1200 queries, 13840 candidates, 12200 swept-bound rejected). Editor PIE numbers carry server+2 client load, so they are upper bounds.
  Rewind in GSTNCombatNP is still O(N) (X4 unchanged).
- Files: Tools/GASNPPTest/minion_scenarios.py, docs/notes/perf-debt.md; edited GSTN_MinionBench.{h,cpp}, GSTN_MinionCombatSubsystem.{h,cpp}, GSTN_MinionHitTests.cpp.
- Not run: full GSTN automation after the edits (only the build; minion code paths unchanged except bench and counters), plugin-off build, movement timelines,
  trapped-minion bug, docs in combat.md.
- PERF-DEBT: see docs/notes/perf-debt.md (X4 rewind O(N); pooled skeletal actors; full net state per frame; history ring copies; EnsureSnapshot with no
  fighters; per-hit view rebuild; bench per-tick tag lookup; fixed-time harness waits).
- Cost: ~$4-6 (Sonnet subagent, ~60 tool calls, 3 builds incl. one failed, 5 PIE harness runs).

## G6a (laptop, 2026-11-07): code DONE, gate green, late_join green at 0 % and 2 %; G6b (desktop) NOT started

- Branch `claude/project-thread-oi94cf` from `claude/project-thread-aj375g` (103b75f). G6 split with the coordinator's OK (cost over $15):
  G6a = code, tests, late_join, gate here; G6b = push model ini (needs owner OK, plan §6), R1 bake-off, R2, `scenario_minion_dormancy_flush` 10 min, on the desktop.
- Step 0 on 103b75f: on 1343/0, off 1307/0, 0 ensures, no fix needed.
- Opus writer from `Saved/M8G6/writer-brief.md`, report `Saved/M8G6/writer-report.md` (not versioned). Only `Plugins/GSTNMinion` and
  `Tools/GASNPPTest/minion_scenarios.py` changed; Runtime Build.cs adds NetCore (public). Config ini untouched.
- Done: `AGSTN_MinionPack` (16 items, `FGSTN_MinionNetArray`, push-based props, works with push model off), `MutatePack()` (wake/flush, then
  mutate, MarkItemDirty, push mark; all-T3 pack -> DormantAll), per-item GCRA tier limiter on fixed frames (15/5/1/0 Hz, pack ceiling first),
  budgeted relevancy (rank: attacking, hit in 63 frames, distance; 8000 cm; 64 items or 40 KB/s split over connections at 20 B/item; 1 s
  hysteresis; over-inclusion counter in `GetNetStats()`), registry hash on one always-relevant `AGSTN_MinionNetManager` (error log + counter, no kick),
  client `UGSTN_MinionClientRegistry` (decoded items for G7), `gstn.Minion.Net.Dump`. Codec struct renamed `FGSTN_MinionNetWireItem`.
- Deviations: type sent on every update (9 bits; type-on-add needs a per-connection baseline); "token holder" approximated as "member running a
  move"; `MaxRelevantPerClient` is a CVar (no minion settings class yet); `bTypeHasExt` fixed false.
- Gate: on 1352/0 (1343 + 9 `GSTNMinion.*.Net.*`), off 1307/0, 0 ensures. Harness `minion_late_join` (dedicated + 2 clients, 60/15): 0 % PASS 4,
  2 % PASS 4. Not a true late connect (harness has none, and no third client): clients join the pack channels at the first grant after the spawn.
- Gate script bug found and fixed (`Saved/M8G6/gate.ps1`): after an `-Off` run, restoring the .uproject kept its old timestamp, so UBT reused the
  minion-off makefile and the next "on" build compiled nothing. Now the restore touches the file. (Step 0's on run came before off, so it is valid.)
- Not done (G6b): push model ini, R1 lean-actor variant and numbers, R2 wake latency, dormancy_flush 10 min, 3 headless clients, over-inclusion in
  the bench report. Also still open: trapped-minion bug (G4), movement timelines, docs.
- PERF-DEBT (also in docs/notes/perf-debt.md): NetPublish quantizes all members per pack-frame + linear FindItemIndex; relevancy pass is
  connections x members distance checks; pack destroy/respawn churn; sync TryLoad of types on clients; type bits on every update.
- Cost: G6a ~ $8-10 (Opus writer ~280k tokens, 70 tool calls; my context ~60k; 3 gate builds + 2 writer builds, 2 headless runs, 2 harness runs).
