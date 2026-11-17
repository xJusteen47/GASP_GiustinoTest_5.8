# dair V10 + dropkick bounce, ROUND 3 (last pose round), thread dair-dropkick-anim (desktop, _ue59, 2026-11-17), branch claude/project-thread-98dml5
Review r2 notes + answers: docs/notes/reviews/<move>/review-r2.md; packages in */r3/ (8 grids incl. game-camera zoom, mp4, frames.md). Round 1/2 assets untouched.
**Push of 716d492c**: a normal `git push -u origin claude/project-thread-98dml5`, no --force, no amend: a78cec35 is its parent (`git merge-base
--is-ancestor a78cec35 716d492c` true) and the remote-tracking reflog shows two plain "update by push" entries (a78cec35 then 716d492c). The
"forced update" seen on origin is not from this machine; nothing on the branch was rewritten.
**Dair R3** (KIT_Test_OverlordDAirV10R3, TL_Overlord_DAirV10R3): windows internal 0-2 LightR3, 3-13 HeavyTuckR3, 14-30 HeavyR2, 31-42 LightR3 (lag 4/16/16/4).
The tucked window runs to internal 13 (designer f4-14, review asked f4-12): the review's case touches down on anim 11-12 but the landing is decided
2 frames later (latency), attempts 2/2 (variant_r3b: internal 12 -> heavy; variant_r3c: OK). HeavyTuckR3 (card overlord_dair_land_heavytuck_r3):
f0 = ball on the floor (pelvis 25 cm, measured v86-87), feet slide to 48 cm f1-f2, pelvis 17-22 cm to f9, rise f9-f15 to standing by v103-105;
montage plays to its end (blend_out_trigger 0, out 3 f): with the automatic trigger the rise was mixed with the base layer's crouch (variant_r3c v100-103).
LightR3 (12 f): crouched contact (pelvis 40), upright by f4; attempt 1/2: covers 9 frames, then the BASE LAYER's own landing crouch still shows
(pelvis 84 -> 64 from v95): ABP/locomotion side, code queue (not montage data).
Late light window f32+: dair_land_apex (pressed falling at 240 cm) lands at anim ~23: not reachable with the hub's jump; no higher platform used.
**Dropkick R3** (KIT_Test_PlaytestTBR3, card overlord_dropkick_techbounce_r3): clip f11 = GroundHit f1 (its first frame on screen), f12-14 = f2,
f9-f10 interpolated. Hand-off step now ~10 cm (hands 44.7 -> 35.5, whiff); GroundHit's own f1 -> f2 slam (~30 cm, 1 frame) remains: it is inside the
reaction clip (DA_ReactionAnims, owner asset). Owner decision: a 3-4 f blend-in / entry drop on the Knockdown rule for this chain, or accept.
No C++ this round (tests from round 2: 1744/0, Saved/Logs/ci/test_dair_bounce_r2.log.ue.log). Scripts: setup_overlord_dair_v10.py (DAIR_V10_ROUND=R3),
setup_dropkick_tech_bounce.py (DROPKICK_BOUNCE_ROUND=R3). Owner kit/TL_Overlord_Dropkick unchanged this round (option A from round 2 stays).
Attempts: tucked window 2/2, light base-layer 1/2 (stopped: not montage data), tucked blend-out 1/2 (OK). Lock released at the end.
**FINAL (after Fable r3 POSE PASS on all clips, review-r3.md)**: setup_playtest_dropkick_knockdown.py with PLAYTEST_DROPKICK_OWN_BOUNCE=R3: TL_Overlord_Dropkick
BounceAbility = GA_Overlord_DropkickTechBounceR3, registered in KIT_Test_Playtest TriggeredAbilities; checked in Saved/DropkickBounce/playtest_r3 (see report). KIT_Test_OverlordDAirV9 unchanged; V10R3 is its own kit.
**Game code queue**: (1) 1-frame capsule-restore order vs landing/bounce montage: dair tucked v88 pelvis -12, dropkick low hit v141/142 -35, high whiff
r3 v140 pelvis -32.7 (r2 7.5: regression, timing dependent; variant_r3/dropkick_whiff frames.jsonl); (2) ABP base-layer landing crouch after a landing-lag
montage (12+ f after LightR3); (3) 2 f touchdown latency, then bring the tucked window back to designer f4-12. Polish: hold the tucked pelvis ~25 cm f3-f9.
Final check: Saved/DropkickBounce/playtest_r3 (KIT_Test_Playtest: AM_OverlordDropkickTechBounceR3 v140 whiff / v142 low hit, then GroundHit); tests `Run-Timed.ps1 -Preset Test -Tests GSTN` 1744 Success / 0 Fail (Saved/Logs/ci/test_dair_bounce_final.log.ue.log); no C++ since round 2: the _ue59 build is current.
