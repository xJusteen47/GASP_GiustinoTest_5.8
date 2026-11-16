# Weak point precision, code slice 1 (desktop, 2026-11-09)

Branch `claude/desktop-weakpoint-1` from merge tip dbda3046. Plan: `docs/architecture/plans/precisione-weakpoint-2026-11-09.md` §1-§3 (items 1-3 of §7).
Doc: `Plugins/GSTNCombatNP/README.md`, bullet "Hard surface".

**Done**
- Timeline data: `StrikeKind` (Unspecified/Swing/Stab), `HardSurfaceBehaviour` (Auto/FinishThenRecoil/ImmediateRecoil), `FGSTN_SweepDeflection`
  (bEnabled, MaxDeflectAngleDeg 60, MaxDeflectFrames 8), `GetActiveEndFrame()`. Validator warning: Stab with > 12 active frames on ImmediateRecoil.
- Sim: `FGSTN_CombatSyncCore::HardContact` (victim, hurtbox, timeline frame, root-space normal at 1/127, case, pending recoil; 1 bit while unset).
  First ricochet of a FinishThenRecoil move writes it instead of the target ring; that hurtbox is skipped for the swing; Root/BakedSocket hitboxes
  slide (authored motion minus its part into the face, recomputed each tick; too steep = blade stops, hitboxes closed); soft/weak hurtbox met while
  sliding = normal hit. Case A (|N·flat normal| below cos of `gstn.Combat.DeflectFlatAngleDeg` 35) -> recoil starts on the first frame past the
  active end and replaces the recovery (AdvanceClock); case B -> no recoil. Recoil hook = existing `FGSTN_RicochetResponse` (RecoilMontage, RecoilFrames).
- Stab-only hurtboxes: `bStabOnly` + `StabMaxAngleDeg` (30) on `FGSTN_Hurtbox` and `FGSTN_ExtraHurtbox`. `UGSTN_HitShapeUtils::OutwardNormal`.
- Tests `GSTNCombat.NP.Weakpoint.*` (4): defaults, slide reaches the weak point (and not without slide), case A/B, steep stop, recoil timing with hitstop, wire + reconcile, stab-only.

**Choices made (owner can overturn)**
- Unspecified moves keep the immediate recoil, so existing content and tests do not change until a move is tagged Swing.
- The case-A "pushback impulse" is the recoil's own movement (`RicochetResponse.Movement`), at recoil start, not a separate impulse at contact.
- Slide after MaxDeflectFrames: offset kept, path follows the authored motion again. WorldAtMoveStart hitboxes do not slide.
- No protrusion validator for weak points yet (needs the parts editor; author rule in the plan stands).

**Not done (next thread)**: §4 hand realign (presentation, needs owner anim), §5 right-stick steering, §6 reticle + aim arrow + per-mode flag, §7 gyro.
`EGSTN_AimPolicy`/`MaxTurnDegrees` are data only today: nothing in the sim turns the character at startup, so §5 needs a new input field
(right stick in `FGSTN_InputCmd`/sample) plus a predicted turn through Mover. Estimate unchanged ($8-13 for 5-7).

**Gate**: builds `Saved/WP1/build-b1.out` (46 s), `build-b2.out` (5 s), first try. Headless `Saved/WP1/headless-h1.log`: 1470 / 0 fail / 0 ensures (+4).
Harness (listen+2, 60/15, map DefaultLevel_Harness, logs `Saved/WP1/r*.log`), same subset on base dbda3046 vs branch:
0 %: ricochet PASS/PASS, command_set PASS/PASS, kit_without_dodge FAIL/PASS, hit_basic FAIL/FAIL, hit_trade FAIL/FAIL (extra entity 9 on the map
gets hit in the "control" phase: environment, same on base). 2 %: ricochet, hit_lagcomp, hitstop_montage PASS; hit_host FAIL on both;
dodge_feet base 1/1, branch 2/3 (no ricochet in it, identical code path: watch as 2 % flake). Full SUITE_DEFAULT on DefaultLevel is red on
this machine on base-level things (pawn falls, dashes and jumps never fire: sample floor content missing in the _m7 worktree) - not usable here.
Driver note: UE's Python rejects nested quotes in f-strings (cost two dead runs).

Cost: ~150k context, Opus, estimated ~$9-11. Builds 6 (~45 s with header changes, 5 s otherwise), headless 1 (57 s), harness ~7 runs.
