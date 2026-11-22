# M11 slice 19: L7d the dome pushes back whoever hits it from outside (laptop, 2026-11-13)
Branch `claude/project-thread-wmlass` from `29247edf` (L7c tip, `claude/project-thread-360h11`). Code `b9b95017`.
Owner rule 2026-11-13: the dome blocks every hit from outside AND pushes back whoever hits it from outside. L7c only recoiled.
**Done**:
1. Data on `FGSTN_BackpackDomeTrack` (category `Dome|Pushback`): `PushbackSpeed` (700 cm/s, 0 = no push), `PushbackFrames` (12),
   `PushbackDirection` (`AwayFromCenter` default, or `BackAlongAttack` = minus the attacker's facing): both behaviours as a setting.
2. Fragment carries them (speed in 10 cm/s steps 1 B, frames|direction 1 B, only while a dome is up); the snapshot blob carries them in its
   2 padding bytes (still 40 B, static_assert). No core blob change, no Project Settings, no inputs.
3. `FGSTN_BackpackDomePush` (GSTNBackpackNP): finds the blocking dome again from the snapshot the hit was found against (shell = carrier's
   dome; body inside domes = first dome not holding the attacker's root, same rule as the step) and computes the push on the attacker's plane.
4. Fighter attacker: backpack tick extension `AfterExecute` -> `PushBlockedAttacker`: first Ricochet hit of the frame, not
   `HitType.Projectile`, view snapshot (lag-comp) else step snapshot -> `UGSTN_GE_PushBack` (same move as the shield pushback), on top of
   the recoil. Re-simulated frames queue it again like any move.
5. Minion attacker (PvE): new additive `IGSTN_AttackerPushExtension` / `FGSTN_AttackerPushRegistry` in CombatNP
   (`Simulation/GSTN_AttackerPush.h`); minion subsystem asks it on a ricochet and calls `LaunchMinion(..., bPushBack)`. A minion's slide
   fades with its own `PushBackDeceleration`, not the dome's frames (noted in code).
**Gate (laptop)**: build ok first try. Headless GSTN **1580/0**, 0 ensures (+2: `GSTNBackpack.NP.Dome.PushData`, `.PushVerdicts`;
`Saved/M11L7d/headless-b1.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l7d-all.txt`).
**di / hit_on_wall_hit (L7c first run)**: red in 1 of 25 suite reports in `Saved/R31` (only `report-m11l7c-all.txt`), green on its rerun
and green here in the full run. Failures were a rollback count (median 2 vs max 1) and a wall-grab count (3 vs 4): timing, and the
dome code is a no-op without a dome. Not caused by L7c as far as the evidence goes; keep an eye, do not add them to the flaky list yet.
**Decisions (recommended, flag)**: push added on top of the recoil (not instead); one push per frame; default direction out of the dome.
**Not built**: nobody saw it in PIE; dome visual; harness scenario `backpack_dome`; minion push length per dome frames.
**Manual**: artifact "Cose da fare a mano" v34 updated (backpack entry: assets exist, dome blocks and pushes, tune in the asset).
Cost: ~120k context main thread, no subagents (build/tests were scripts printing 2 lines); estimated ~$10-14. Wall clock ~1.2 h.
