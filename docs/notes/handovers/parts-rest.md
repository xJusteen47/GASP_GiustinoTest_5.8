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
