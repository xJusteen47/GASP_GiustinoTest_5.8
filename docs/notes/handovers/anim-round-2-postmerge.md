# ANIM ROUND 2 post-merge + CP2 desktop build (overnight desktop, UE 5.9, 2026-11-15)
Folder `GASP_Giustino2_ue59`. Editor closed, no map saved, PM_BoostJump/PM_ThrowRider restored after each run. `AM_OverlordDAirV9.uasset`
and `Apri editor 5.9.bat` left dirty as before. **_ue59 is BUILT on `claude/project-thread-anim-r2-int`** (checked out).
- **CP2 (`claude/project-thread-6bl38a`)**: compiled first time, no errors. GSTN run crashed in `GSTNCombat.NP.Weakpoint.SetFacingEffect`
  (pre-existing since 08338b8d: `FApplyMovementEffectParams` raw pointers uninitialized, effect turns a garbage UpdatedComponent). Test fixed
  (null the pointers), pushed `3579f2c3`; then 1614/0 headless (`-nullrhi`). n216qs had made the same fix independently.
- **Integration `claude/project-thread-anim-r2-int`** = anim-r2 `fa79d89b` + 6bl38a `3579f2c3` (merge `1fd56be2`) + n216qs `8ba977e1`
  (merge `fa0b48fa`; one conflict, the same test fix, kept CP2's). Full build OK, GSTN 1615/0. Reviews commit on top (see git log).
- **Captures (post-merge)**, mp4 + grids for the rule-10 cloud review:
  `docs/notes/reviews/overlord-jab-v3-r2/` (8 variant mp4 + g0-g4 regenerated; base untouched), `overlord-dropkick-r1/` (2 mp4 + g1),
  `overlord-dair-v9-codefix/` (base + variant mp4, g1 grid). Each folder's numbers.md / README has a POST-MERGE section.
- **What I saw (one look per grid, not the review)**: jab end squat STILL THERE post-merge (v82-88 deep crouch, same as base: GASP pose
  match on the wide jab 2 exit). Jab first-frame montage ticks still unstable (faf run: jab 2 2.09 on v34 AND v35). Dropkick unchanged:
  lies flat on the floor through the land clip, no get-up. DAir variant vs base almost identical; differences only around the exit v70-76.
- Pre-merge jab frames kept in `Saved/AnimR2/jab/variant_old0215`, dropkick in `variant_premerge`. New helper `make_grid_dropkick_post.py`.
- **Next**: cloud rule-10 review of the three folders; code thread for the jab exit squat (exit pose vs GASP pose match) and the dropkick
  loop-hold/land/get-up; CP3 can start from 6bl38a.
- **Cost**: one Opus thread, about 120k context, no subagents; estimate $12-15 plus about 25 min of desktop build/capture time.
