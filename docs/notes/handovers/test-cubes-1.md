# Handover test-cubes-1: fixed cubes of different hardness to attack (2026-11-25)

Branch `claude/project-thread-k3aapg`, base `main` 152d25e6. Written in the cloud; built on the desktop 2026-11-25 23:2x UTC in the worktree GASP_Giustino2_testcubes: Succeeded, 1242 s, no errors, no fixes. Verified on the desktop 2026-11-26: 12 `Content/` junctions, `GSTNParts.Minion` tests (`11/11 PASS`), and PIE standalone (`gstn.TestCubes.Spawn` + hits + `Report` + screenshots).
Owner request 2026-11-25: "Programma qualche superficie tipo cubi a penetrazione diverse. Fissi che posso attaccare."
Reading taken: "penetrazione diversa" = the Helldivers rule already in GSTNParts (hit penetration against part hardness: full, 65 %, ricochet).

## What
- `gstn.TestCubes.Spawn [0,1,3,6,10,15] [Distance=350] [Spacing=200]`, `gstn.TestCubes.Clear`, `gstn.TestCubes.Report` (not in Shipping).
- `UGSTN_TestCubesSubsystem` + `AGSTN_TestCubePuppet` in GSTNPartsMinion (`Debug/GSTN_TestCubes.{h,cpp}`): each cube is a minion of a transient
  type (one per hardness, reused) with one unbreakable part on one box hurtbox; fixed by data (speeds 0, body collision None, reaction capped
  to StaggerInPlace, KnockbackScale 0, health 1e6). Puppet = engine cube coloured by hardness + text label (hardness, band of the last hit, damage).
- Bridge observer `UGSTN_PartsMinionSubsystem::OnPartOutcomeApplied` (multicast, raised at the end of ApplyOutcome). Inert when unbound.
- Reuse: minion store and hit flow (GSTNMinion), part math and PartResolveMinion (GSTNParts/GSTNPartsMinion), spawn pattern of `gstn.SpinScrew.Spawn`.
  No new plugin, no .uproject change (GSTNPartsMinion, GSTNParts, GSTNMinion already enabled).

## Desktop steps (one thread, _ue59 lock)
1. Editor closed. Merge `claude/project-thread-k3aapg` into the day's integration branch.
2. `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` (new UCLASSes: full build).
3. PIE standalone on the playtest map, console `gstn.TestCubes.Spawn`. Expected: 6 cubes in a row, green to red, label "Durezza N".
4. Hit each one: a move with penetration 0 does 65 % on hardness 0 and "RIMBALZO" on the others (log `TestCubes hit ...`). Set a hitbox's
   Part Hit penetration in the timeline editor to see the full band.
Max 2 fix attempts (rule 2).

## Desktop verification (2026-11-26, `GASP_Giustino2_testcubes`)
- Worktree aligned `c2678dd6 -> 9375fe02`; pre-existing build verified intact (`Saved\Logs\ci\build.log.esito`: `ESITO: OK`, `1242 s`, `UnrealEditor-GASP_Giustino2.dll` + `UnrealEditor-GSTNPartsMinion.dll` + 77 top-level / 97 recursive plugin DLLs).
- Created the 12 NTFS directory junctions in `Content\` targeting `GASP_Giustino2_ue59\Content\*` (`git status -sb` clean).
- Automated tests (`.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-cubes.log -Tests GSTNParts.Minion`, validated with `.\Tools\CI\Check-TestLog.ps1 -UeLog Saved\Logs\ci\test-cubes.log.ue.log -MinTests 11`): `ESITO: OK` (`40 s`), `ESITO TEST: PASS  ok=11 fail=0 totale=11 (minimo 11)`.
- PIE standalone on `/Game/GSTN/Maps/L_Test_Hub` (`.\Tools\CI\Run-Timed.ps1 -Preset Capture -Log Saved\Logs\ci\pie-testcubes.log`, `ESITO: OK`, `67 s`, `driver=OK`):
  - `gstn.TestCubes.Spawn` spawned 6 `AGSTN_TestCubePuppet` actors in a row (`350 cm` ahead, `200 cm` spacing) with camera-facing labels `Durezza 0, 1, 3, 6, 10, 15` and HSV gradient green `(0.1, 0.8, 0.15)` -> red `(0.85, 0.1, 0.1)`.
  - Hit all 6 cubes with `KIT_Test_Hits` (`Penetration = 0`); `gstn.TestCubes.Report` and `Saved\Logs\ci\pie-testcubes.log.ue.log` confirm:
    - `hardness=0`: `hits=3 ricochets=0 damage=14.3` (`band=1, ricochet=0`, label `pen - durezza >= 0: 65%, danno 5.2`).
    - `hardness=1, 3, 6, 10, 15`: `band=2, ricochet=1, damage=0.0` (`hits=4/4/4/4/3, ricochets=4/4/4/4/3`, label `RIMBALZO`).
    - Zero displacement (`delta_xy = 0.0 cm` on all 6 cubes); screenshots visually inspected in `Saved\Logs\ci\testcubes_spawn.png` and `Saved\Logs\ci\testcubes_after_hits.png` (+ `Saved\Logs\ci\testcubes_pie_summary.txt`).
- `_ue59` locks (`C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59\_ue59\IN-USO.txt` and `IN-USO.txt`) acquired from `LIBERO` and restored to `LIBERO`.

## Not done / to confirm
- Remote clients do not draw the cubes (transient types). Fine for standalone/host playtest.
- The owner's moves probably all have penetration 0 (default): to confirm in the editor; the per-move values (and testing a hitbox with `Penetration > 0` in the timeline editor) are a design choice of the owner.

BRANCH: claude/project-thread-k3aapg
ESITO: PASS compilato (1242s), test GSTNParts.Minion 11/11 verdi (40s), prova PIE TestCubes superata (67s: 6 cubi verde->rosso, 65% su Durezza 0 e RIMBALZO su 1..15)
SUBAGENTI: 2 explorer + 1 worker (M2 k3aapg)
COSTO: ~4.5 $ | 23 min desktop | 0
