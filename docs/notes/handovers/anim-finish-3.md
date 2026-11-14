# ANIM-FINISH-3 handover: reaction fixes and first Play captures (desktop, UE 5.9, 2026-11-14)
Branch `claude/project-thread-j8zkjb` (from 0kekzb `3faa4695`), folder `GASP_Giustino2_ue59`. No C++ changed: still BUILT; editor closed,
no map saved. Read before: `anim-finish-2.md`, reviews react-v1 / react-v2. Owner OK'd the forward rock-up roll (11-14 03:00Z).
**Done: HitStand rebuilt, RollBack reworked, travel note, montages, Play captures of all clips. Not done: Quaternius, dropkick, dair, jab.**
- **HitStand**: authored + built from the card of the last thread (chest twist -26, trailing right arm low), game copy replaced. Build
  worst 1.2 cm. Seen only in the Play sheets (not by me frame by frame): the cloud review judges it.
- **RollBack**: new keys f4/f6/f8/f10 (`reaction_rollback.plan.json`): pelvis roll 12/30/42/32, chest side bend up to 15, head turned left;
  in the plan the right shoulder is the low point f6-f8 (z 4-7), head bone 19-25 cm up f6-f10 (16 at f11), both hands planted. Build worst
  4.1 cm. In the Play capture from behind the legs pass to the RIGHT of the head and the head stays visible on the left (I looked at one
  12-frame sheet from behind and one from the side of run 1, and the behind sheet of run 2).
- **Travel note**: `docs/notes/reviews/react-v3/roll-travel.md` (Roll: 0 until f6, 0.7 by f10, 1.0 by f15; RollBack: 0 until f4, f5-f12,
  the clip implies about 65 cm). For the code thread; the abilities are unchanged.
- **Montages** `AM_React_{GroundHit,GetUp,Roll,RollBack}` in `Content/GASNPPTest/Combat/Reaction/` (new). GroundHit blend in 4/60, out 0.
  The other three: blend in 0, blend out 8/60 with trigger time 0 (the clip plays to its last frame, then blends). With trigger -1 the
  blend started 10 frames early and the game's pose replaced the rise.
- **Play capture**: `Tools/GASNPPTest/pie_capture_react.py` (new; boot file = one line `exec(open(<script>).read())`, launch line in its
  header). L_Test_Hub, host pawn, clips played on the character's anim instance, IN PLACE (no ability, no travel). Six sequences:
  hitstand, tumble, splat (wall 30 cm behind the root), knockdown_getup (Tumble > GroundHit > LieUp > GetUp), roll, rollback; views
  behind, right side, game; 2 minutes per run. It preloads the montages before Play (loading inside the Play tick failed, cause unknown).
- **Review material** `docs/notes/reviews/react-v3/`: per sequence `<seq>.mp4`, `<seq>_slow.mp4` (0.25x), `<seq>_az180_sheet.jpg`,
  `<seq>_az90_sheet.jpg` (every frame from f8; `v` = shot frame, `a` = montage frame). **Rule 10 NOT met by me** (three sheets seen).
- **OPEN, game side (2 attempts used, stopped)**: after GetUp / Roll / RollBack end standing (pelvis 90.5, head 149.5 on the last clip
  frame), the game's own pose drops into a hunched crouch within 4-8 frames (pelvis 70-73, head 115-122) and comes back slowly (pelvis 89
  only 25 frames later). Same with either blend setting, so it is the game animation blueprint after a montage that started lying down
  (suspect: motion matching picks a crouch/land clip from the pose history), not the clip. Check whether a plain jab montage does it too.
- **Known gaps**: `clip_grid.py` showed the idle pose on every frame for both rebuilt clips (capture chain: the Play capture shows the
  right clips): do not trust it until checked. Splat wall, Tumble > GroundHit blend, GroundHit floor f2-f3: captured, not judged.
- **Quaternius**: zip at `...\Unreal Projects\_scratch\Universal Animation Library[Standard].zip` (not extracted; `Unreal-Godot/*.glb`). Not started.
- **Next thread, in order**: (a) cloud review of react-v3, (b) the crouch after the montage (game side), (c) Quaternius retarget trial,
  (d) dropkick, (e) down-air v9 blend-out, (f) jab v3 round 1.
- **Cost**: one Fable thread at about 150k tokens of context, 3 Sonnet subagents from exact briefs (113k, 85k, 102k tokens, all right
  at the first try; Antigravity not used); estimate about $30. Editor startup still rewrites `PM_BoostJump` / `PM_ThrowRider`: reverted.
