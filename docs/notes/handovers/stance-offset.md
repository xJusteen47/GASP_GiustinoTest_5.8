# STANCE-OFFSET handover: upper-body stance offset after moves, option A (desktop, 2026-11-16)
Branch `claude/stance-offset` from `claude/night-knockdown` c64d0471. Folder `_ue59`, Opus. Editor built (Run-Timed Build OK), ABP saved
(backup `Saved/Backups/GAS/stance/`). Evidence in `docs/notes/reviews/stance-offset/`. Brief: `handoffs/ritorno-idle-ricerca.md` option A.
## What was done (isolated, foot-lock untouched)
- `UGSTN_StanceDefinition` (GSTNAnimationCore): StancePose minus ReferencePose (plain sequences, same skeleton, one sampled time each), per-bone
  weights + fade-time scale (default spine_01..05 0.3-0.5-0.7-0.85-1.0 at 1.3x, neck 1.0 at 1.2x, clavicles/arms 1.0 at 1.0x; pelvis, legs, root
  never), and the per-kit settings: `ReturnTiming` WithFeet | BeforeFeet, `ReturnDelay` (FFXIV-style, default 0), FadeInTime 0.25, FadeOutTime 0.15.
- `FGSTN_AnimNode_StanceOffset` + editor node, in the main AnimGraph right after the foot lock (`abp_diff.txt`: only that node and its two links).
  Pass-through (input evaluated, nothing touched) when no stance or stance hidden. `a.GSTN.StanceOffset 0` = off.
- `UGSTN_CharacterAnimInstance`: `DefaultStance`, `SetStance()`, timing on the game thread. Stance only at rest: hidden while a montage drives
  (yields with 1 - montage weight), dropped over FadeOutTime while the capsule moves (> 10 cm/s), back after the trigger + ReturnDelay.
  WithFeet = montage weight 0 and no foot in the Locked phase; BeforeFeet = montage weight 0. Test cvar `a.GSTN.StanceTest <asset path>|none`.
- Inertialization: not requested. The fades are smoothstep (no pop at either end), and a stance switched while shown blends from the node's
  last output (SwitchBlendTime 0.2). A graph inertialization request would also catch GASP's legs. Decision applied (rule 6).
- Per kit: `FGSTN_StanceKitExtension` (kit/weapon extension list) + `UGSTN_KitStanceComponent` (project module, listens to OnLoadoutApplied).
  The component is NOT on SandboxCharacter_Mover yet (shared Blueprint); no kit names a stance yet.
- Test asset `/Game/GASNPPTest/Stance/DA_Stance_TestRelaxed` = M_Relaxed_Stand_Idle_Loop over M_Neutral_Stand_Idle_Loop. Same skeleton
  SK_UEFN_Mannequin, both AAT_NONE, all listed bones present (`Saved/Stance/setup/stance_setup.json`, from `Tools/GASNPPTest/stance_setup.py`).
## Results (`python Tools/GASNPPTest/stance_rows.py <dirs>`, `footlock_rows.py <dirs>`; captures pie_capture_move.py, on = StanceTest cvar)
- jab1_long, on vs off (`rows_long.txt`, `footlock_rows_long.txt`): same timeline (montage ends f45, lock f46, steps f85, free f99) and same
  foot-lock numbers (slide L 0.3 R 0.6, step L 11.0 R 12.8 both runs). Stance hidden f21-f85, return starts f86 (with the steps), torso done ~f106.
- Locomotion (`rows_on_off.txt`): walk_into and jump_only show state 4 (moving, stance out) on every moving frame, stance alpha while moving 0.0.
  Idle feet with the stance on: foot_l [1.4,-15.2,-90.5] vs off [1.3,-15.3,-90.8]. Hands/head move (hand_r y 26.7 -> 18.8 cm): the stance shows.
- Per-frame diffs on vs off (`diff_on_off.txt`) are not meaningful: sessions are not frame-aligned (actor spawns 9 m apart, GASP picks differ).
- Looked at (rule 10): `jab1_long_az45_on_off.png` + `jab1_long_az45_on_vs_off.mp4`. Legs identical, arms/torso drift to the relaxed pose
  f86-f106, no pop seen. Not an owner PASS: the stance asset is a test, not a design choice.
## Fix attempts: 1 (crash in RebuildCache: GetPoseBoneIndexForBoneName returns a mesh index, now converted to compact; seen on a 41-bone LOD).
## Open / for the owner
- Default ReturnTiming = WithFeet (applied); BeforeFeet and ReturnDelay are coded but not captured in game.
- Add `UGSTN_KitStanceComponent` to SandboxCharacter_Mover (shared BP) and give kits a stance: needs the real chosen idles first.
- A jump apex can briefly count as "still" (jump_only f62-64, alpha < 0.01): harmless now; a grounded check would need Mover data.
- `AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. `IN-USO.txt` set, then freed. Cost: ~7 $ estimated.
