# Handover: greybox capsule, rounds 3-6 (thread "Test greybox capsule on desktop", 2026-11-24)

Branch `claude/project-thread-gaov87` (from `claude/project-thread-mswztw` c6e4eff, main merged in at the end). Full notes and every
owner note: `docs/notes/greybox-capsula.md` (sections Round 3 to Round 6, Desktop runs 3 to 6). Owner 2026-11-24 17:07 Rome: "the capsule
is fine for now".

What changed (all presentation only, behind `gstn.Greybox 1`):
- Squash about the ground on the pelvis, never a non-uniform root scale (it sheared the X-up bones: crouch through the floor).
- Body and nose rigid on the pelvis (no skirts), red bigger hand markers, run start sway lerped to `FranticAmplitude` 0.45.
- Per-sequence move clips (dropkick startup / loop / land, tech bounce), jab 2, dair landings; Playtest jab plays `AM_OverlordJab1V3`
  (owner 2A, `Tools/GAS/greybox/set_playtest_jab_montage.py`, BP backed up first).
- Jump squat, stronger landing, plant against the motion in any direction, wall slide (lean off the wall, friction stick-slip judder,
  held brake lean).
- Kit look on the capsule (`UGSTN_GreyboxSubsystem`): collar in the kit's colour, greatsword flat on the back for Overlord kits only,
  both stored in pelvis space.

Evidence: desktop runs 3-6, build OK each time, greybox tests all pass (Core.Greybox + NP.Greybox, run logs in the "Desktop run N"
sections); videos `docs/notes/reviews/greybox-capsula/round3..round6/`.

Not committed, owner-local on the desktop `_ue59`: `DA_Locomotion_Default` (run start ramp), `CHT_GSTN_StockMover_PoseMatch`,
`IC_GASNPP_Test`, and `M_GSTN_Greybox` / `MI_GSTN_Greybox_Grey` resaves with no intended change. Owner stash `fb95b1e1` untouched.

Open (not asked by the owner yet): wall judder strength is small, to judge at real speed; Overlord moves without a montage (ftilt,
fthrow) get capsule clips when their montages exist; the Content reorg (`claude/project-thread-e7z54v`) will move `/Game/GSTN/Greybox`
after this merges (paths are constants in `GSTN_GreyboxSubsystem.h` and the two greybox scripts).

ESITO: PASS capsule approved by the owner for now; rounds 3-6 built, tested and seen in game on the desktop.
SUBAGENTI: none (one Remote Control session on the desktop, same model).
COSTO: not measured | ~75 min desktop (runs 3-6) | 0 review tokens (4A: placeholder clips, no level-2 review)
