# ANIM-FINISH-2 handover: forward roll, back roll over a shoulder (desktop, UE 5.9, 2026-11-14)
Branch `claude/project-thread-0kekzb` (from yhnbvx `bf82599d`), folder `GASP_Giustino2_ue59`. No C++ changed: the playtest folder is
still BUILT; editor closed, no map saved. Brief: `anim-finish-brief.md`. Previous: `anim-finish-1.md` (how the clips are made).
**Done: forward roll + back roll rework (editor captures). Not done: Quaternius, dropkick, down-air v9 blend-out, jab v3, any Play capture.**
- **`AS_React_Roll` is now the FORWARD roll** (24 f, in place, the ability gives the travel toward the stick): knees whip to the chest
  (f4), trunk rocks forward along the back with a left hand push (f8), feet plant in a deep squat (f11), weight rolls over the feet in a
  low tuck (f14), rise (f18), idle (f23). My choice: a rock-up roll, NOT a full somersault (from supine that is 450 degrees, about 36
  frames; say so if the owner wants it). Plan `cards/reaction/reaction_roll.plan.json`.
- **`AS_React_RollBack`** = the old back roll, renamed (`reaction_rollback.*`), reworked after the react-v1 review (FAIL: weight on the
  neck): pelvis roll 18-20 degrees and head turned at f8-f10, so it goes over the RIGHT shoulder; lowest point is a hand, head 4 cm up.
- Both built (`python -m animpipe author <card path>` then `build <card path>`: the verbs take the card PATH), game copies saved in
  `Content/GASNPPTest/Combat/Reaction/` (Roll replaced, RollBack new). Build measure vs source: Roll worst 6.9 cm (calf_r), RollBack 4.1.
- **Review material** `docs/notes/reviews/react-v2/`: `Roll.mp4`, `RollBack.mp4` + sheets (editor captures: right, back, 3/4 front).
  I looked at one 4-view grid of both clips (f0,4,8,10,12,14,18,23): no broken limbs, floor ok, both read from behind. I did NOT watch
  every frame of the mp4 and there is no Play capture: rule 10 is not met yet, the cloud review must look at the videos.
- **HitStand (review fix) is only half done**: plan + card edited (chest twist -26, pelvis twist -12, trailing right arm dropped behind
  the hip, left arm thrown forward), stick preview looked at, but the clip is NOT rebuilt and the game copy is still v1. Next thread:
  author + build `cards/reaction/reaction_hitstand.json`, 6-view grid, game copy.
- **Other react-v1 review notes, not started**: Tumble->GroundHit blend 3-4 f, Splat against a real wall, GroundHit floor at f2-f3,
  GetUp blend to idle, all seven captured IN GAME (back + side). The in-game hook for Hitstun/Tumble/Splat/lying still does not exist.
- **Quaternius: NOT downloaded.** Owner said yes (11-14 02:41Z), but the free file is behind itch.io's "No thanks, just take me to the
  downloads" button; the scripted cookie+POST route was denied by the permission system and I did not force it. Asked the owner to save
  `Universal Animation Library[Standard].zip` into `C:\Users\jacop\Documents\Unreal Projects\_scratch\quaternius\download\` (folder
  not created yet; no `_scratch` exists on this machine). CC0, FBX/GLB/Blend. Then: extract, inventory, retarget, try on reactions.
- **Subagents worked well for mechanical steps**: one Sonnet agent did editor start, author, build, blank map, captures, game copies and
  editor kill in 4.5 minutes from an exact brief. Antigravity not used this time (nothing needed it). Blank map: `ue_builtin.execute`
  with `new_blank_map(False)` (last expression must be a json string), `PYTHONPATH=Tools/AnimPipeline`.
- In passing: editor start rewrote `PM_BoostJump`/`PM_ThrowRider` again (restored). `Apri editor 5.9.bat` is untracked on purpose.
- **Next thread, in order**: (a) HitStand rebuild + react-v2 review fixes + in-game captures, (b) Quaternius once the zip is there,
  (c) dropkick (poses in the Antigravity worktree, read-only), (d) down-air v9 blend-out, (e) jab v3 round 1.
- **Cost**: one Fable thread at about 150k tokens of context (most of it the fixed start-up context), 2 Sonnet subagents (about 80k and
  86k tokens), no Antigravity calls; estimate about $25.
