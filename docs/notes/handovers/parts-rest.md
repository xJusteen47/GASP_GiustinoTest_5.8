# Parts leftovers (M4b P15-P19) handovers

## P18 hurtbox openings + per-hurtbox hardness override (laptop, 2026-11-08): DONE, gate green

- Branch `claude/p18-openings-laptop` from e4ac5b3. Opus writer (medium), brief `Saved/P18/brief.md`; design notes in m4b-doc-deltas.md "P18".
- `FGSTN_HurtboxOpening { bEnabled = false, Mouth, MaxPassRadiusCm }` on `FGSTN_Hurtbox` (additive, existing sets unchanged).
- Deviation from the plan: the opening rule is NOT in `UGSTN_HitStep_PartResolve` (that step only reads the attacker's chosen HurtboxIndex and
  has no hitbox shape). It runs where the overlap list is built, in the attacker tick (`GSTN_CombatTick_Attacker.cpp`, GSTNCombatNP), through
  the pure helper `UGSTN_HitShapeUtils::PassesOpening` (GSTNCombatCore): a hurtbox with an opening counts as overlapped only through its mouth
  and only if the hitbox's pass radius fits (combat-m4-hits §9c B). Inert while no hurtbox enables an opening.
- `HurtboxHardnessOverride` (TMap FName -> 0..15) on `FGSTN_PartDefinition`, read by PartResolve; damage pool and break stay the part's.
  Validator: error outside 0..15, warning for an unknown hurtbox key. Editor: the mouth is drawn in magenta in the hurtbox set editor.
- Tests: `GSTNParts.Core.Openings.*` 7 (CavityReachedOnlyThroughMouth, BulkyHitboxFallsToArmour, ProjectileRadiusFits, NoOpeningKeepsPriorityRule,
  OverrideWinsOverPartHardness, EitherSideBreaksTheWholePart, RicochetPredictionUsesOverride).
- Gate: all on 1416/0 (+7), GSTNMinion off 1324/0, GSTNPlatform+Parts off 1394/0, 0 ensures; SUITE_DEFAULT on DefaultLevel 50/50 (0 %).
- Not done: openings on the minion attacker path (GSTNMinion untouched), a hurtbox-set validator rule for openings, the editor balance report
  with the override, TM3 leg content, wounds (low priority, plan).
- PERF-DEBT (also in perf-debt.md): one bool per touching hurtbox without an opening; mouth test (radius first, then shape overlap, swept when
  the hitbox sweeps) only on opened hurtboxes; override: one emptiness check per part hit, a map lookup only on parts that use it.
- Cost: ~$6 (Opus writer ~190k tokens / 61 tool calls; 4 builds incl. 2 off configs, 3 test runs, 1 SUITE_DEFAULT).

## P15 GSTNPartsMinion interface (laptop, 2026-11-08): DONE, gate green; P16 next

- Branch `claude/p15-parts-minion-laptop` from 44e71da. Owner decision 2026-11-08: minions lose parts and keep fighting (Helldivers-style).
  Sonnet writer (`Saved/P15/brief.md`, `report.md`, which lists what P16 must do).
- New plugin `Plugins/GSTNPartsMinion/` (deps GSTNParts + GSTNMinion, EnabledByDefault false, NOT in the .uproject), empty Runtime module.
- GSTNPartsCore (pure, no M8 include): `bForMinions` on the part set + `FGSTN_PartsValidation::ValidateForMinions` (> 8 parts and `Stats`
  break effects rejected; the deferred MinionMaskOver8 rule); `FGSTN_PartsMinionExtension { PartSet, LethalParts }` (default: no part loss is
  lethal, a headless minion keeps fighting; a MainDamage `Kill` flag is ignored for minions, with a warning); `FGSTN_PartsSnapshotExt::
  FromMinionStore / ToMinionStore` for a 9 B row {BrokenMask, Damage[8]} (no actor per part; P16 puts it in the minion store and the G10 ExtByte).
- Tests `GSTNParts.Core.Minion.*` 5. Gate: all on 1421/0 (+5), GSTNMinion off 1329/0, GSTNPlatform+Parts off 1399/0, 0 ensures.
  Canary `make_canary.py GSTNPartsMinion --tests GSTNParts.Core.Minion`: builds alone, 5/0. No SUITE_DEFAULT (no NP or combat change).
- PERF-DEBT (also in perf-debt.md): at 160 minions the part state is 9 B x 160 = 1.4 KB of store; wire = the G10 ext byte only for
  part-bearing types and only on change; per-hit step cost to measure in P16 against the 2 us budget.
- Cost: ~$4 (writer ~153k tokens / 23 tool calls; 3 builds, 3 test runs, 1 canary).

## P16 sub-gate A (laptop, 2026-11-08): minion part state + hit flow DONE, gate green; B (wire, promotion, scenario) next

- Branch `claude/p16-parts-minion-laptop` from ceeed86c. Opus writer (medium), brief `Saved/P16/brief-a.md`, report `Saved/P16/report-a.md`.
- Bridge `GSTNPartsMinion` (still disabled in the .uproject): `UGSTN_PartsMinionSubsystem` (9 B row per slot + type byte + generation; columns
  sized at the first part-bearing activation, slot reset on activate, freed slots guarded by generation), `UGSTN_HitStep_PartResolveMinion`
  (in the minion rules' extra victim step slot; runs the fighter part math on the store row, no GE, no fragment), `UGSTN_PartsMinionSettings`
  (type tag -> extension) and `RegisterDefinition` at run time. A broken part kills only if it is in LethalParts (head loss keeps fighting).
- GSTNMinion: 2 additive seams, inert unbound: `VictimViewExtension` (end of MakeMinionView) and `ApplyOutcomeExtensions` (bool delegate before
  the death test; true = health 0 + normal death path).
- Gate main project: all on 1423/0, GSTNMinion off 1330/0, GSTNPlatform+Parts off 1401/0, 0 ensures. Canary `GSTNPartsMinion --tests GSTNParts`:
  builds alone, 153/0 incl. `GSTNParts.Minion.{StoreOutcomeNoGE, HeadLossKeepsFighting, LethalPartKills, NoExtensionNoCost, ResetOnFree}`.
- Doubts (report): HealthWeight is re-run to divide its scale out; the bridge takes the single extra step slot; Launch/Stagger part effects left to B.
- PERF-DEBT (in the bridge README): 160 minions x (9 + 1 + 4) B columns ~2.2 KB; per hit: one row read/write + the fighter part step (to measure in B).
- Cost: ~$7 (Opus writer ~238k tokens / 74 tool calls; 3 main builds incl. 2 off configs, 3 test runs, 1 canary).

## P16 sub-gate B (laptop, 2026-11-08): wire, puppets, promotion DONE; plugin left DISABLED (2 % numbers not clean)

- Commit 42a93304 on `claude/p16-parts-minion-laptop`. Opus writer (medium), brief `Saved/P16/brief-b.md` (report in the lead's notes).
- Wire: part-bearing types get `bHasNetExt`; BrokenMask goes out through the G10 `SetExtByte` only on change. Puppets
  (`UGSTN_PartsMinionPuppetSubsystem`) hide broken part bones (host from the server row, clients from `GetExtByte`). Promotion: the bridge seeds
  the fighter's parts through new `FGSTN_PartsAttach::SetPendingSeed/ConsumePendingSeed` (GSTNPartsNP, consumed once at AfterClocks).
  GSTNMinion: 2 more inert seams `OnDefinitionRegistering`, `UGSTN_MinionClientRegistry::OnDefinitionResolving` (bHasNetExt fixed before the wire format).
  Bench: `gstn.PartsMinion.Bench.Parts <TypeTag> <hurtboxes> <cap>`, `gstn.PartsMinion.Bench.Report`; scenarios `minion_parts_base` + `minion_parts`
  (SUITE_MINION_BENCH; PASS "bridge not enabled" when off).
- Gate (plugin off, committed state): all on 1423/0, GSTNMinion off 1330/0, Platform+Parts off 1401/0, 0 ensures; canary bridge alone 156/0.
- Trial with the plugin ON (not committed): build ok, all on 1431/0 (+8). SUITE_MINION 0 %: 9/11, same as today (lagcomp 1 cancel, lod known).
  `minion_parts` 0 %: works (23/40 heads broken, 23/23 headless alive and attacking, client ext bytes = server masks) but FAILS two budgets:
  bytes +11.9 % per client (target +5 % + 2 % noise) and 5.06 us per part hit (target 2 us: view 0.19, **step 4.72**, outcome 0.15).
  Puppets: 23 hiding but `missingBones` 23 (the bench puppet mesh has no `head` bone: the visual is NOT verified).
  SUITE_MINION 2 %: 8/11, then rerun of the two: player_hits 1 cancel (G7b case), lagcomp no swing (flaky input). Not "today's numbers" -> kept OFF.
- Not done: Launch/Stagger part effects (needs a GSTNMinion merge seam with the drain's knockback), part events via the outbox, demotion copy.
- Doubt: `bHasNetExt` is set at runtime on the definition object (stays set after PIE in the editor; would be saved if someone saves that asset).
- PERF-DEBT @160 minions: memory 160 x 14 B ~2.2 KB; wire 1 B per item per mask change (+5 % of a 20 B update only on that update); server
  ~5 us per part hit, the part step 4.7 us of it (fighter math reused: cache the part set lookup and skip the full snapshot for minions);
  puppet bone hide per change only. With 160 minions and ~10 part hits per frame at peak: ~0.05 ms/frame.
- Cost: B ~$9 (Opus writer ~323k tokens / 109 tool calls; 4 builds incl. trial, 3 test runs, canary, 4 harness runs). P16 total ~$16.
- Next: (1) cut the part step cost (cache) to <= 2 us; (2) a bench puppet with a head bone (or a test mesh) for the visual; (3) clean 2 % runs
  of the two flaky scenarios, then enable GSTNPartsMinion in the .uproject.

## P16-C (laptop, 2026-11-08): step cost 5.06 -> 2.75 us, puppet bones fixed; target 2 us NOT met in 2 attempts -> plugin stays DISABLED

- Branch `claude/p16c-parts-enable-laptop` from c529525f. Sonnet writer (`Saved/P16/brief-c.md`).
- Cost: per (part set, rules) step cache (pipeline flags, bands, min weight scale; dropped on register/initialize), HealthWeight inline, no
  request written; attempt 2 builds the outcome in place. Bench (40 minions, 0 %, plugin on in a local trial): hitUs 5.06 -> 2.85 -> **2.75 us**
  (view 0.21, step 2.25, outcome 0.29). `gstn.PartsMinion.StepCache 0` = old path; test `GSTNParts.Minion.CachedStepSameOutput`.
- Bytes: the G10 codec writes the 8-bit ext byte in EVERY item of a part-bearing type (not only on change): +8/119 bits = **+6.7 % per update
  floor**; measured +9.9 % (updates x1.03). Cheaper alternative (PERF-DEBT): a 1-bit presence flag so intact minions pay 1 bit (GSTNMinion codec change).
- Puppets: bench puppets had no mesh at all (the bench mesh cvar never reached them); the bridge's puppet subsystem now gives a mesh-less
  part-bearing puppet `gstn.PartsMinion.Bench.PuppetMesh` (SKM_Manny, has `head`): **missingBones 0**, broken heads hidden (headless check).
- Gate with the plugin on (trial): all on 1432/0. Committed state (plugin off) unchanged from c529525f except the bridge and the scenario.
- Not done: SUITE_MINION 2 % x3 with the plugin on (pointless while (1) fails). To reach 2 us: precompute per-hurtbox part index + band in the
  cache so the step is array reads, and/or skip GetDamageProgress for minions (estimate ~0.5-0.8 us); codec presence bit for bytes.
- Cost: ~$5 (writer ~200k tokens; 2 trial builds, 1 test run, 2 bench runs).

## P16-C close (laptop, 2026-11-08): GSTNPartsMinion ENABLED in the .uproject

- Coordinator decision: 2.75 us per part hit accepted for now (owner's rule: prototype first). PERF-DEBT kept: per-hurtbox part index + band
  precomputed in the cache (~0.5-0.8 us); 1-bit presence flag in the minion codec (bytes floor +6.7 %).
- SUITE_MINION 2 % x3 with the plugin on (dedicated + 2, 60/15): run 1 9/11 (lagcomp: no swing, lod known), run 2 10/11, run 3 10/11 (lod
  known only). Plugin-off reference today: 9/11. Never worse -> enabled.
- Headless all on with the plugin enabled: 1432/0, 0 ensures. Off-config gates not rerun (only the .uproject entry and docs changed since the
  P16-C gate).
- Docs: m8-doc-deltas.md and traversal-doc-deltas.md folded into the READMEs of GSTNMinion, GSTNPartsMinion, GSTNSurface, GSTNPlatform,
  GSTNPlatformParts (Haiku; the notes are kept as history with a header line).
- Cost: ~$3 (3 SUITE_MINION runs, 1 test run, doc fold).

## P18b (laptop, 2026-11-08): openings on the minion attacker path + opening validator DONE, gate green

- Branch `claude/p18b-openings-laptop` from ad449ee9. Sonnet writer (`Saved/P18/brief-b.md`).
- Minion -> fighter overlap loop (`GSTN_MinionCombatSubsystem.cpp` ~line 1985) applies `UGSTN_HitShapeUtils::PassesOpening` like the pawn tick
  (one bool when no opening). Player -> minion already goes through the pawn attacker tick (no separate minion-victim overlap list).
- Validator: `FGSTN_CombatRulesValidation::ValidateHurtboxOpening` (GSTNCombatCore), called from `ValidateHurtboxSet` (so IsDataValid and the
  editor rules validator report it): errors for MaxPassRadiusCm <= 0, a mouth with no volume, a mouth not overlapping its hurtbox grown by 2 cm.
- Tests: `GSTNCombat.Core.Hurtbox.{OpeningOutsideBoundsRejected, OpeningZeroSizeRejected, ValidOpeningAccepted}`,
  `GSTNParts.Core.Openings.MinionAttackerRespectsOpening` (runs a copy of the loop body: the real loop is inline in the subsystem).
- Gate: all on 1436/0 (+4), GSTNMinion+GSTNPartsMinion off 1334/0, GSTNPlatform+Parts off 1414/0, 0 ensures. SUITE_DEFAULT 50/50.
- SUITE_MINION 0 %: 9/11 (player_hits no swing = flaky input, lod known). 2 %: 7/11, rerun of the 3 flaky: player_hits PASS, lagcomp no swing,
  **reaction_tiers FAIL twice** ("still items 0, read T1/T2 0": client tier inference; known flaky since G8, but two in a row here: watch it;
  P18b cannot reach it while no hurtbox has an opening). lod known.
- Not checked: how `GSTN_CombatValidators.cpp` maps the new messages to its `Hurtbox.*` codes.
- PERF-DEBT: none new (one bool per touched hurtbox on the minion path; validation is editor/load time).
- Cost: ~$4 (writer ~117k tokens; 4 builds incl. 2 off configs, 3 test runs, SUITE_DEFAULT, 3 SUITE_MINION runs).

## P19 screws (laptop, 2026-11-08): DONE, gate green

- Branch `claude/p19-screws-laptop` from merge f f0c528ab. Sonnet writer (`Saved/P19/brief.md`, `report.md`).
- `TurnSense` / `TurnSteps` on `FGSTN_HitData` (default 0: no hit turns anything; shown in the hit data editor under "Screw");
  `FGSTN_PartScrewRule`, `HeldBy` / `HeldByRule` on `FGSTN_PartDefinition`. Turn counter = the screw's existing damage byte (+ceil(255*steps/
  TurnsToRelease) in the loosen sense, minus with floor 0 the other way): no new bytes. Turn rule and held-by in `UGSTN_PartMath::ApplyHit`
  (a ricochet band still returns first; a released plate breaks in the same hit). Deviation to confirm: a held part's damage byte is capped
  at 254 while its rule is unmet, so damage alone cannot break it (the spec text only defines the release).
- Minions: screws allowed (the 9 B row holds the byte; the minion step calls the same ApplyHit; its cache reads no new data).
- Validation: bad sense, TurnsToRelease outside 1..255, unknown/empty/self HeldBy, cycles through HeldBy and ParentIndex.
- Tests: `GSTNParts.Core.Screws.*` 6 (spec §8.1) + `ValidationRejectsBadScrewAndHeldBy`. 1 lead fix: the hit data editor customization lists
  its properties (test `ExtensionDataNotHiddenByCustomization` failed until TurnSense/TurnSteps were laid out).
- Gate: all on 1443/0 (+7), GSTNMinion+GSTNPartsMinion off 1341/0, GSTNPlatform+Parts off 1421/0, 0 ensures. SUITE_DEFAULT 50/50.
  SUITE_MINION 0 % 9/11 (lagcomp 1 cancel, lod), 2 % 9/11 (lagcomp no swing, lod): known reds only.
- Not done: spin/drill hitboxes authoring TurnSense (GSTNSpinDrill), TM3 plate content, turning-screw presentation, balance report, HitDataOverride fields.
- PERF-DEBT (also in perf-debt.md): screw/held-by passes only on parts that have a rule (one check per part hit otherwise).
- Cost: ~$5 (writer ~194k tokens; 5 builds incl. 2 off configs, 4 test runs, SUITE_DEFAULT, 2 SUITE_MINION).
