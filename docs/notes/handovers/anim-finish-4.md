# ANIM-FINISH-4 handover: down-air blend-out, jab v3 round 1, dropkick poses (desktop, UE 5.9, 2026-11-14)
Branch `claude/project-thread-anim4x` (from a5idak `9d92bfec`), folder `GASP_Giustino2_ue59`. No C++ changed: still BUILT; editor closed, no
map saved. **Nothing here is ready for the owner: rule 10 review still to do on the two review folders below.**
- **Down-air v9 blend-out: target NOT met, 2 attempts used, stopped.** `docs/notes/reviews/overlord-dair-v9-blendout/` (base + variant mp4,
  grids v64-v81, `numbers.md`). FAF = anim 42, clip 43 f. Blend starting at the clip end (trigger 0) moved the leg jump from v78 to v80-81
  and made it bigger (7217 px side); 16-frame blend: 3911 px. In every run the legs jump on the first frame after `inTimeline` goes false
  (ability end): cause is game side (leg layer / fall state when the ability ends in air), not montage data. Montage left at 8 f / -1.
  Kit script gained `OVERLORD_DAIR_BLEND_OUT_TRIGGER` (default unchanged). Next: a small code thread on what switches at ability end in air.
- **Jab v3 round 1: built, kit refreshed, captured in Play.** `docs/notes/reviews/overlord-jab-v3-r1/` (per sequence base/variant/slow mp4,
  grids base on top, `numbers.md`). Cards `cards/overlord/jab/overlord_jab{1,2}_v3.json` (r0 backups beside them, notes
  `overlord_jab_v3_round1_notes.md`). Jab 1: contact 40 deg forward-right (hand 44 cm right of centre, 31 cm outside the shoulder from
  behind), knees 12/20, hold drifts 4.4 cm, new f1 key (feel rule). Jab 2: **step 18 cm in the pose, no root motion: capsule z 100.0 and
  displacement 0.0 on every frame** (base: z 88.1, 30 cm), windup rises f3-f6, hand at chest height (z 126), f52 = idle.
- **Jab, known defects for round 2 (not judged on video by me: I saw no jab frame; the subagent saw one small grid):**
  (a) jab 2 built in-betweens are off the plan (worst 23.9 cm on hand_r, hand speed peak at f12 instead of f9: `contact_faf` FAIL): add
  keys on f7, f8, f10-f12; jab 1 worst 10.3 cm; (b) jab 1 variant f25-f26 shows the body bent low where base shows idle: check the clip
  end / blend-out; (c) jab 1 hand visually misses a dummy straight ahead (44 cm right, 35 cm short) while the fixed hitbox still hits;
  (d) jab 2 rear knee 29 deg (wanted 15-20), trunk upright (plan: 30 deg forward).
- **Dropkick (Fair): poses written, NOT built.** `cards/overlord/dropkick/overlord_dropkick_{startup,loop,land}.json` + `.plan.json`, stick
  previews, `overlord_dropkick_notes.md`. Tuck from f0-f4, legs release f12-f15 (46 cm on f15 = first active), loop moving hold (seam
  1.6 cm), land on the back ending exactly on `reaction_lieup` at f16. I looked at the startup and land stick previews (side, behind, top).
  The Antigravity worktree had nothing usable (cards identical to ours, renders show idle). Quaternius: 43 clips, no dropkick/lying/get-up.
- **Next thread for the dropkick**: copy `cards/overlord/dropkick/timing/*.csv` to `Saved/AnimPipeline/refs/ufd/`; `animpipe author` + `build`
  per card (card PATH); `assemble_dropkick_montage.py`; kit `KIT_Test_OverlordDropkick` on the model of `setup_overlord_dair_kit.py`; Play
  capture. Game side needed: loop held until landing (script plays it 3 times), loop-to-land cut (pelvis drops 53 cm), shrinking capsule.
- **Choices applied, owner to confirm**: jab 2 step 18 cm in pose space with net travel 0; jab 1 diagonal 40 deg (alternative if the miss
  bothers: 25 deg, or lean the body left so the arm reaches a centred target; or move the hitbox +30 right, reach 65); dropkick arms thrown
  back and out, asymmetric, instead of the old `_design` "arms crossed on the chest" (crossed arms failed legibility from behind);
  dropkick `part_delays` 0. Still open from before: the 12 cm right-foot stomp in jab 1.
- **Left dirty on purpose**: `Content/GASNPPTest/Combat/Overlord/AM_OverlordDAirV9.uasset` shows modified (values equal to the committed
  ones, binary differs; a subagent's `git checkout` of it was denied by the permission system and I did not do it for it). Not committed.
  `Apri editor 5.9.bat` untracked, not mine. `PM_BoostJump` / `PM_ThrowRider` restored after each editor run.
- **What worked**: pose design to two Opus-high subagents in parallel while the editor was busy; build + Play capture to Sonnet from exact
  briefs (base captured BEFORE the build so base and variant share frames); `vd2_mover.py` put the windows on desktop 2 (hr=0).
  `animpipe check` needs the timing csv in `Saved/AnimPipeline/refs/ufd/` (copy from `cards/.../timing/`).
- **Cost**: one Fable thread at about 150k tokens of context; subagents Opus high 209k + 325k tokens, Sonnet 141k + about 150k; estimate $40-45.
