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

## G7a (laptop, 2026-11-07): presentation code DONE, gate green; scenarios 2/5 green; G7b (desktop) NOT started

- Branch `claude/project-thread-n4yotz` from `claude/project-thread-oi94cf` (6271c2b). G7 split with the coordinator's OK (estimate over $15):
  G7a = code, tests, 5 scenarios, gate here; G7b = spike 7 + owner video on the desktop (with G6b).
- Opus writer from `Saved/M8G7/writer-brief.md` (report `writer-report.md`, not versioned); Sonnet for the scenarios (`scenarios-report.md`).
- Done (only `Plugins/GSTNMinion` + `minion_scenarios.py`): `UGSTN_MinionPresentationSubsystem` (client registry or, on a listen host, the store),
  pooled `AGSTN_MinionPuppet` (null mesh ok headless), `FGSTN_MinionInterpolator` (Core; T0 on NP `Interpolation` render frame, T1/T2 dead
  reckoning + 200 ms blends, teleport snaps; tier inferred from sample gap), `MaxVisible` 60 nearest first (CVar `gstn.Minion.Present.MaxVisible`),
  predicted flash (custom primitive data 0) + mesh pause, confirmed by HitCounter in 500 ms or cancelled, puppets as client hittables (pack
  replicates a small `NetItemSlots` table for slot -> server id), URO + OnlyTickPoseWhenRendered, pooled placeholder shatter,
  `gstn.Minion.Present.Dump` (puppet + summary lines, format in writer-report). Definition gains soft `PuppetMesh`, `PuppetAnimClass`.
- Deviations: Animation Budget Allocator skipped (plugin not enabled, logged once). `UGSTN_HitPresentationComponent` cannot present minion
  victims (`FindOwnerActor` null, material param flash): subsystem reuses its tracker on the local pawn's sync state; attacker-side pause and
  local cue do not play for minion hits (needs a GSTNCombatNP hook, not touched). Two old unity-clash helpers renamed.
- Gate: on 1363/0 (1352 + 11 `GSTNMinion.*.Present.*`), off 1307/0, 0 ensures.
- Harness 0 % (dedicated + 2 clients, 60/15): `minion_replicate` PASS 4, `minion_late_join` PASS 5 (now with puppets). FAIL:
  `minion_player_hits_minion`, `minion_lagcomp`: client Attack taps are sent but the server shows lagCompQueries=0, drainedHits=0, client
  predicted=0, even with Bench.Attack minions walking up to both pawns (fix 1: Spawn's 600 cm ring was out of reach). Next: check that the
  tap activates a swing on the harness pawn and that the player->minion hit query is wired (GSTNCombatNP side; G5 only tested minion->player).
  `minion_reaction_tiers`: fix 1 (sample windows overlapped) done; now T0 items get 0.8 updates/2 s vs T1 9.0: tier inversion or wrong
  client tier inference, undiagnosed. 2 % runs NOT done (0 % not green). Max 2 fix attempts reached for the session budget.
- G7b needs from the desktop: spike 7 (listen host that renders + a client, Development, `stat unit`/Insights: game thread ≤ 3 ms at 60
  puppets, ≤ 2 ms at 40, or M8 §4d.1 fallback recorded); owner video of 40 grunts hit (several angles); a puppet mesh/anim class set on
  the grunt definition (none yet); Animation Budget Allocator plugin enable (owner OK) if wanted; the three failing scenarios and 2 % runs.
- Not done: anim-instance driving, ground snap, Launched extrapolation, instanced mesh for culled minions; G4 trapped-minion bug; docs.
- PERF-DEBT (also in docs/notes/perf-debt.md): registry polled per frame; full sort for the visible cut; linear FindRecordBySlot; sync loads
  of puppet assets; puppet transform set every frame even unchanged.
- Cost: G7a ~ $12-14 (Opus writer ~280k tokens, 82 tool calls; Sonnet ~120k; my context ~70k; 2 writer builds + 3 gate builds, 2 harness runs).

## G7a closing (laptop, 2026-11-07): 3 of 5 G7 scenarios green at 0 % and 2 %; player_hits_minion and lagcomp still red (2 fix attempts used)

- Branch `claude/project-thread-xm9w8k` from `claude/project-thread-n4yotz` (cd6cfa9). Diagnosis in `Saved/M8G7/diag-g7a.md` (not versioned).
- Player -> minion, cause harness/bench side, hit pipeline and GSTNCombatNP untouched: (a) a bare harness pawn's Attack has no hit move (needs
  `KIT_Test_Hits`, whose swing tag is `Test.State.HitMove`, not `Attacking`); (b) the bench's transient grunts had no hurtbox set, so nothing could
  hit them; (c) the attackers keep the swinging pawn in hitstun. Fix: bench CVar `gstn.Minion.Bench.HurtboxSet` (read when the transient definition
  is built), harness `_swinger_setup` (kit, `HBS_Test_Pawn` on the grunts, infinite stagger threshold on the pawns).
- `lagCompQueries=0` is by design: no live code calls `GatherRewoundCandidates`; minions are rewound by the GSTNLagComp overlay through the provider.
  The lagcomp check on that counter was replaced by "the kit swing came out".
- Tiers: the server is right (all 40 items at ceiling 15 = T0, near the pawn). The client inferred T0 for still minions forever (last two samples
  close, then silence) and the old check compared raw update counts, which follow motion. Fix: `GetInferredTier(Params, NowSeconds)` counts the
  silence since the newest sample as a gap (unit test added); scenario rewritten (server ceilings by distance, still items read slow, moving items
  update more and within their ceiling) on slow Massacre launches (Chase minions stand still by the samples).
- Results (dedicated + 2 clients, 60/15): 0 % replicate PASS 4, late_join PASS 5, reaction_tiers PASS 4; player_hits_minion and lagcomp FAIL only on
  "none cancelled": predicted 33/36, confirmed 22/30, cancelled 6/6 (server drained hits, swing out). 2 %: replicate, late_join, reaction_tiers PASS;
  player_hits_minion and lagcomp FAIL: the kit swing never came out (no HitMove on the server, 0 drained hits).
- Gate: on 1363/0, off 1307/0, 0 ensures (the tier test grew 3 asserts, same test count).
- Open (next thread, Opus for the first): (1) ~18 % of predicted minion flashes cancelled at 0 % loss: check whether cancels are hits on dying
  minions (3 of 12 die in the run) or client/server pose disagreement; if dying, allow cancels on minions that died in the window. (2) At 2 % the
  scripted one-frame tap seems lost or the loadout not applied: check `Test.State.HitMove` on the client and the loadout view before the taps,
  try `combat_hold` for a few frames. Then rerun both at 0 % and 2 %.
- Files: Tools/GASNPPTest/minion_scenarios.py; GSTN_MinionBench.cpp; GSTN_MinionInterpolator.{h,cpp}; GSTN_MinionPresentationSubsystem.cpp;
  GSTN_MinionTests_Present.cpp.
- PERF-DEBT (also in perf-debt.md): none new in runtime; harness scenarios still wait fixed times (no event waits).
- Cost: ~$7 (one session, ~75 tool calls, 2 builds + gate on/off, 6 headless harness runs incl. 2 diagnostic).

## G8 (laptop, 2026-11-07): AI hook DONE, gate green, spike 6 PASS; 7/8 scenarios green at 0 %, 6/6 at 2 %; minion_lod red (harness side)

- Branch `claude/project-thread-kbrsme` from `claude/project-thread-xm9w8k` (85e74b0). Sonnet writer (`Saved/M8G8/writer-brief.md`, report `writer-report.md`).
- Done (only `Plugins/GSTNMinion` + `minion_scenarios.py`): `UGSTN_MinionBrainSubsystem` runs one `FStateTreeExecutionContext` per brained slot, budgeted by tier
  (T0 every frame, T1 1/4, T2 1/16, Dormant none, round robin), commits one intent per tick; `UGSTN_MinionBrainComponent` is a handle only (no tick);
  schema `UGSTN_MinionStateTreeSchema` (context = `FGSTN_MinionBrainContext` external data), tasks MoveToTarget / FaceTarget / RequestAttack, conditions
  TargetInRange / IsLODAtLeast; `UGSTN_MinionSteeringUtils` (seek, arrive, separation; nav path <= 2 Hz when a navmesh exists, else direct seek);
  native fallback brain (`gstn.Minion.Brain.Native 1`, or a slot with no tree); test tree "chase and swing" built and compiled in code in GSTNMinionEditor
  and handed to Runtime through a factory; LOD wake (inbox hit or launch on Far/Dormant -> Combat, brain ticks that frame, `OnMinionWoken`).
  Bench: `Spawn <N> Brain`, `Attack <N> <Fighter> Brain`, `Poke <BenchIndex>`; `gstn.Minion.Brain.Report|Dump`, `gstn.Minion.LOD.Log 1`. Definition: soft `Brain`.
- Deviations: brain is not a ticking `UStateTreeComponent` per actor (perf rule, announced at start); tier rates are CVars, not the LOD batch intervals;
  StateTree plugin added to GSTNMinion.uplugin (.uproject untouched); FaceTarget got an empty instance struct (5.8 compiler rejects a task without one).
- Gate: on 1374/0 (1363 + 11 `GSTNMinion.*.Brain.*`), off 1307/0, 0 ensures.
- Spike 6 (headless test, 40 minions forced T0, 120 frames): StateTree 4.96 us per minion tick (0.005 ms, threshold 0.02 ms: PASS; one 4.1 ms first-tick
  outlier = tree init), native 0.49 us. StateTree kept; native stays as fallback.
- Harness (dedicated + 2 clients): 0 % tokens_brain, hits_player, tokens, replicate, late_join PASS; reaction_tiers FAIL once ("still items 0") then PASS
  on rerun with no code change in its path (flaky, watch it). 2 %: tokens_brain, hits_player, tokens, replicate, late_join, reaction_tiers all PASS.
  minion_lod FAIL (2 fix attempts used): the "no pop" checks pass, but the tiers never change because the third server pawn `BP_GASNPP_TestCharacter_C_2`
  stays at -800,0 after `server_teleport_exact` (logged in the dump steps), so a controller pawn stays 190 cm from the minions. Next: in `teleport()`
  fall back to `set_actor_location(..., teleport=True)` for any pawn not moved after 0.3 s (or teleport every controller pawn), then rerun lod at 0 % and 2 %.
  lod not run at 2 %. Brain.Dump fixed to one line (leaf state) with action/move/intent/nearestPawn diagnostics.
- Not in scope, still red: player_hits_minion, lagcomp (desktop queue).
- PERF-DEBT (also in perf-debt.md): execution context built per ticked slot; neighbours by store walk (O(N) per minion); target pick scans fighters per slot
  at 2 Hz; tree LoadSynchronous on first spawn; instance-data arrays sized to capacity; Brain.Dump nearest-pawn scan (debug only).
- Cost: ~$12 (Sonnet writer ~350k tokens / 85 tool calls; my context ~90k; 3 builds + gate on/off, 4 harness runs).

## G9a (laptop, 2026-11-07): waves, corpses, promotion server core DONE, gate green; G9b (desktop) NOT started

- Branch `claude/m8-g9-laptop` from `claude/project-thread-kbrsme` (31ed1dc). Split approved: G9a here, G9b deferred. Sonnet writer (`Saved/M8G9/writer-brief.md`, `writer-report.md`).
- Step 0 (harness only): `scenario_minion_lod` records every controlled server pawn and moves any the exact teleport left behind with
  `set_actor_location` 0.3 s later; the no-pop check now measures from snapshot A. Tier chain now PASS (12/12, no Combat->Dormant jump), poke/wake PASS.
  Still red on "no pop" at 0 % and 2 % (1 attempt used): the client puppet overshoots the server path by 1-2 m at Far/Dormant (e.g. client 1039 cm vs
  server 882 cm from A). Presentation side (interpolator extrapolating on rare Far updates), not harness: next thread, clamp extrapolation by tier interval.
- Done (only `Plugins/GSTNMinion`, `minion_scenarios.py`, docs): `CorpseSeconds` on the definition (frame stamp `CorpseEndFrame`, corpse not moved, not brained,
  no hits, freed in the flush); `UGSTN_MinionWaveDefinition` (entries: soft definition, count, spawn point tag, delay frames, ring radius; IsDataValid;
  `GetAssetsToPreload`); `AGSTN_MinionSpawnerActor` (plan name was U-, it is an actor; trigger box, async 2-step preload on server and clients, StartWave,
  GetAliveCount, `OnWaveCleared`, ticks only while a wave runs); `UGSTN_MinionSpawner::Promote` + `IsMinionAlive`: refuses queued/Spawning/Dying/Dead,
  cap `gstn.Minion.MaxFightersFromMinions` 4, spawns the fighter class at feet + half height with a replicated `UGSTN_MinionPromotedComponent`
  (minion item id, pack id, type tag, health fraction, team), sets team via IGenericTeamAgentInterface, frees the minion, native `OnMinionPromoted`.
  Bench `gstn.Minion.Bench.Wave <N> <CorpseSeconds>`, `gstn.Minion.Bench.Kill`; `scenario_minion_wave` in SUITE_MINION.
- Deviations: no GAS attribute write on the fighter (health fraction is on the component, the kit applies it in G9b); `Event_MinionPromoted` is only logged
  (no Event_Minion* is published anywhere yet); acceptance item 3 (type + wave as assets without C++) deferred: no editor asset script run (cost).
- Gate: on 1379/0 (+5 Wave/Corpse/Promote), off 1307/0, 0 ensures. Two test fixes (array self-reference in the preload test; promote test hit a still-queued spawn).
- Harness (dedicated + 2 clients, 60/15): 0 % hits_player, tokens, replicate, late_join, tokens_brain, wave PASS; reaction_tiers FAIL once ("still items 0")
  then PASS on the allowed rerun (flaky, same as G8); lod FAIL (no pop, above). 2 %: all PASS except lod (no pop). player_hits_minion, lagcomp not run (desktop queue).
- G9b (desktop): Promoting flag on the wire + client cross-fade from the component's ids, health fraction applied by the fighter kit (check X4 first),
  `scenario_minion_promote` (no pop > 15 cm, first control <= 500 ms), spike 8 numbers, acceptance item 3 assets, Event_Minion* publishing.
- PERF-DEBT (also in perf-debt.md): corpse list walk per batch; corpses still scheduled/LOD/published/hashed (skip Dead in schedule and hash); spawn points
  by GetAllActorsWithTag per wave start; wave preload not held resident; Promote sync-loads the fighter class when not preloaded; weak fighter list prune.
- Cost: ~$9 (writer ~316k tokens / 65 tool calls; my context ~95k; 5 builds incl. off, 3 gate test runs, 6 harness runs).

## G10 (laptop, 2026-11-07): bridge seams for platforms and parts DONE (inert), gate green in three configurations

- Branch `claude/m8-g10-laptop` from `claude/m8-g9-laptop` (75b3419). Sonnet writer (`Saved/M8G10/writer-brief.md`, `writer-report.md`). Only `Plugins/GSTNMinion` + docs.
- Seams (all inert unbound, GSTNMinion gains no dependency on GSTNPlatform/GSTNParts): (1) `SetPlatformActor(Handle, bool)`: NetPublish skips (or removes,
  then re-adds on clear) the pack item, the store keeps simulating; (2) `IGSTN_MinionPublishObserver` + `Add/RemovePublishObserver`, called once per
  step from StagePublish with the end-of-F pose/yaw/velocity of flagged slots only (count-gated, reused array); (3) per-slot `MaxLODTier` ceiling
  (`SetMaxLODTier`, applied in StageLODUpdate) and `bExemptFromCombatCap` with `IsExemptFromCombatCap` next to the "// G11" demotion comment;
  (4) `SamplePose(Handle, Frame, Pos, Yaw, bClamped)` over FGSTN_MinionHistory (new `FindOldest`); (5) ExtByte: `bHasNetExt` on the definition,
  `SetExtByte`, client registry `GetExtByte`.
- Deviation: the item Format did not know the type, so `FGSTN_MinionNetFormat` gained an `ExtTypes` table filled by a process-wide
  `AGSTN_MinionPack::SetNetExtTypes` (server on registry change, client on registry resolve). With no ext type the wire is the v1 layout (same bits).
  Open risk for P16: a client pack received before the registry would parse with an empty table (only once a type sets bHasNetExt).
- Skipped (optional in the plan): moving the GSTNPlatform pose point to the G1 hook (would edit GSTNPlatform).
- Gate: all on 1386/0 (+7 `*.Seams.*`), GSTNMinion off 1307/0, GSTNPlatform+GSTNPlatformParts off 1364/0, 0 ensures. One build fix: unity build
  merged the G9a wave tests with the hit tests (same anonymous helper names): wave helpers moved to a named namespace.
- Harness (dedicated + 2 clients, 60/15): 0 % and 2 %: hits_player, tokens, replicate, late_join, reaction_tiers, tokens_brain, wave PASS (no rerun
  needed); lod FAIL on "no pop" only (known, out of scope). player_hits_minion, lagcomp not run (desktop queue).
- Gate script: `Saved/M8G10/gate.ps1 -Off -OffPlugins A,B` disables any plugin list (GSTNPlatformParts must go off with GSTNPlatform).
- PERF-DEBT (also in perf-debt.md): observer pose copy per flagged slot per step; process-wide ext-type table read per item NetSerialize (cache per pack);
  NetPublish tests the platform flag on every activated slot each frame (fold into the dirty pass in G11); SamplePose FindOldest scan (rare).
- Cost: ~$6 (writer ~221k tokens / 43 tool calls; my context ~+40k; 5 builds incl. 2 off configs, 3 gate test runs, 2 harness runs).

## G11a (laptop, 2026-11-07): host budgets DONE (MaxActiveT0 cap, T0 rate fallback, horde/bytes scenarios), gate green; G11b (desktop) NOT started

- Branch `claude/m8-g11-laptop` from `claude/m8-g10-laptop` (e742a53). Split (told the coordinator): G11a here, G11b needs the desktop as host that renders.
  Sonnet writer (`Saved/M8G11/writer-brief.md`, `writer-report.md`). Only `Plugins/GSTNMinion`, `minion_scenarios.py`, docs.
- Cap: cvar `gstn.Minion.MaxActiveT0` (48, 0 = off), one `ApplyT0Cap` pass after the StageLODUpdate loop: farthest non-attacker Combat slots -> Near,
  bounded selection, tie by slot; spared: attackers by state, token holders (`HoldsAny`, only when over the cap), `IsExemptFromCombatCap`, a ceiling
  at Combat. Candidates' tier events are emitted after the loop (Near->Combat->Near = no event). `GetActiveT0Count`, `GetT0DemotedCount` (cumulative).
- Fallback 2: cvar `gstn.Minion.T0RateHz` (15, clamp 5..15) pushed from NetPublish to Core `SetCombatRateHz`; default = same bytes. Fallback 3 (puppet cap) not touched.
- Bench: Report prints a parseable `MinionBench budget ...` line; new `gstn.Minion.Bench.Population N Near FarRadius [Idle|Chase|Brain]`.
- Deviations: per-definition `FGSTN_MinionLODParams::MaxActiveCombat` left unused (the cvar is the host cap); the token-holder case has no unit test
  (the batch re-resolves tokens each frame), it is exercised by minion_horde; G9a perf-debt "skip Dead in hash" not done (hash feeds observers/lag-comp).
- Gate: all on 1394/0 (+8 `*.Budget.*`), GSTNMinion off 1307/0, 0 ensures. One fix round: demoted count made cumulative, exempt test re-timed.
- Harness (dedicated + 2 clients, 60/15, `GSTN_HORDE_SECONDS=20`): 0 % and 2 %: hits_player, tokens, replicate, late_join, reaction_tiers, tokens_brain,
  wave, horde, bytes PASS (no rerun); lod FAIL on "no pop" only (known). `SUITE_MINION_BENCH` = horde, bytes (not in SUITE_MINION).
- Provisional laptop numbers (-nullrhi, NOT the §11 measurement): horde 60 minions, active T0 capped at 48, batch avg 0.52 ms / max 3.4 ms.
  Bytes (planning: client item updates x 20 B): N=40 ~0.1 KB/s per client, N=80 ~1.6-2.0 KB/s (t0 48 / t1 32). The N=40 figure is for minions
  standing at their targets (updates follow motion): G11b must measure a moving horde and wire bytes (`stat net`), 3 headless clients.
- FINDING: the 150-population case spawned only 128 (store capacity 128): awake 60, dormant 68. A 150 population needs capacity >= 150
  (one store setting + the column sizes); decide in G11b/G12, not changed here.
- G11b (desktop): listen host that renders (Development, `stat unit`, Insights `GSTN_MinionBatch`), max N honestly, R4/R5 recorded, §8.3/§11 re-based,
  moving-horde wire bytes with 3 clients, fallbacks of §4d.1 in order if the frame does not fit, capacity for 150.
- PERF-DEBT (also in perf-debt.md): deferred candidate events + O(over x candidates) selection; HoldsAny linear walk; T0 counted from scheduled slots only;
  silent re-demotion churn after the Near dwell (hold-by-cap bit); bytes scenario fixed waits; G10 platform-flag line fixed only partly.
- Cost: ~$7 (writer ~205k tokens / 56 tool calls; my context ~75k; 4 builds incl. off, 3 gate test runs, 2 harness runs).

## G12a (laptop, 2026-11-07): capacity 160, editor validators, debug cvars, forced-reconcile scenario, docs DONE, gate green; G12b (desktop) NOT started

- Branch `claude/m8-g12-laptop` from `claude/m8-g11-laptop` (77a37e2). Split (told the coordinator). Sonnet writer (`Saved/M8G12/writer-brief.md`, `writer-report.md`).
- Capacity: `gstn.Minion.Max` was already a cvar (columns sized by capacity, per-frame walks over alive/scheduled lists = flat CPU): default 128 -> 160
  (`GSTNMinion::DefaultCapacity`). Memory ~2.4 KB per slot, +32 slots ~78 KB (history ring ~70 %). 150-population now spawns 150 (awake 60, dormant 90).
  `MaxWaveTotal` stays 128 per wave.
- Validators: pure Core checks `GSTN_MinionValidation` (registry: null/duplicate/duplicate TypeTag/over 256; definition refs; wave cross-asset incl. over
  capacity) + Editor DataValidation validators for definition and wave (`DataValidation` added to the uplugin and Build.cs). No registry asset exists (the
  table fills in first-spawn order), so the registry rule runs over a wave's distinct definitions.
- Debug: `gstn.Minion.Debug.{Capsules,Hitboxes,LOD,History,Intent}` (`GSTN_MinionDebugDraw`, not in Shipping, ticks only while one is on).
  Deviation: `.Hitboxes` draws the playing move's reach sphere, not the timeline shapes. `GetSlotWaypoint` added on the brain subsystem.
- Harness: `scenario_minion_forced_reconcile` in SUITE_MINION. One fix: the "forced" log count is not required (client ForceReconcile lines do not reach
  this log in dedicated + 2; the rollback check proves the forces), bounds unexplained 0 / mismatches 40 / burst 20 kept.
- Gate: all on 1398/0 (+4 `*.G12.*`), GSTNMinion off 1307/0, GSTNPlatform+Parts off 1376/0, 0 ensures. SUITE_DEFAULT not rerun (no NP or combat code touched).
- Harness (dedicated + 2 clients, 60/15): 0 % and 2 %: hits_player, tokens, replicate, late_join, reaction_tiers, tokens_brain, wave, forced_reconcile
  (0 % after the fix, on a rerun of that scenario only), horde, bytes PASS; lod FAIL "no pop" only (known).
- Docs: README "How to extend"; testing.md minion suites; new `docs/notes/m8-doc-deltas.md` (G0-G12a deltas + combat.md folds for G12b).
- PERF-DEBT (also in perf-debt.md): debug pass cost while a cvar is on; history ring ~70 % of slot memory even for slots that never write it (allocate
  per awake slot); sync loads in the validators (editor only).
- Cost: ~$7 (writer ~214k tokens / 68 tool calls; my context ~+40k; 4 builds incl. 2 off configs, 3 gate test runs, 3 harness runs).

### M8 laptop status (2026-11-07): G0-G12a done on the laptop; desktop items left
1. G6b / G7b: performance numbers on the desktop as listen host that renders (replication and puppets).
2. G9b: Promoting flag on the wire, client cross-fade, fighter health from the component, `scenario_minion_promote`, spike 8, acceptance item 3 assets, Event_Minion*.
3. G11b: rendered host frame, max N, R4/R5, §8.3/§11 re-based, moving-horde wire bytes with 3 clients, §4d.1 fallbacks if needed.
4. G12b: listen + dedicated sweep of every §12 scenario; SUITE_PARTS/DEFENCE/LINK in three configs; byte/CPU tables; combat.md folds (m8-doc-deltas.md); §13 checklist; TM3 brief.
5. `scenario_minion_lod` "no pop": client puppet overshoots at Far/Dormant (clamp interpolator extrapolation by tier interval).
6. `scenario_minion_player_hits_minion` and `scenario_minion_lagcomp`: red since G7a, desktop queue.
7. X4: lag-comp rewind iterates all minions (GatherRewoundCandidates not wired into GSTNCombatNP): needs an owner OK to touch GSTNCombatNP.
8. `scenario_minion_dormancy_flush` (§12) not written yet.
9. Perf-debt review of docs/notes/perf-debt.md at the end of M8 (many G6-G12 lines), with the low-end target.
10. Merge path: laptop branches g9 -> g10 -> g11 -> g12 are stacked; nothing pushed to feature.
