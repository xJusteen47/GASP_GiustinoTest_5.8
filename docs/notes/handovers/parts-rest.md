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
