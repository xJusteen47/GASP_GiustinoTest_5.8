# PUSHBOX-P3 handover: pushbox P2 in game, high-percent dair + dropkick 275 cm (desktop, 2026-11-16). DONE (captures), knockdown still not seen
Branch `claude/pushbox-p3` from `claude/pushbox-p2` 4f81e809, folder `GASP_Giustino2_ue59`. Sonnet-tier thread, no subagents. Not pushed (owner approval in thread).
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No C++, no build, no asset/map/setting saved. `_ue59/IN-USO.txt` set then freed.
## Change (script only, `Tools/GASNPPTest/pie_capture_move.py`)
- Test-only percent: the EXISTING server-only `UGSTN_GASNPPTestComponent::ServerSetDamagePercent` (test harness component, not shipping gameplay)
  called after the dummy is placed when a sequence has `dummy_percent` (env `PIE_CAPTURE_DUMMY_PERCENT`, default 180). No C++ needed.
- New sequence `dair_knockdown` (520 frames, dummy 62 cm, `walk`: stick forward from press+50 for 400 frames). Rows now carry the dummy's
  step snapshot (`snap`: actionState, reactionFlags, damagePercent). `dropkick_hit` dummy 120 -> 275 cm (env `PIE_CAPTURE_DROPKICK_DIST`).
## Proof (captures `Saved/Logs/ci/pushbox_p3_cap_dair.log.esito` 150%, `..._cap_dair2.log.esito` 180%, `..._cap_dropkick.log.esito`: OK)
Numbers: `python Saved/pushbox_p3/st.py <frames.jsonl>` and `pass.py` (copies of the one-off readers, not committed) on `Saved/pushbox_p3/cap_*`.
- 150% (try 1): hit, launch ~1700 cm/s up-forward, no tumble flag, dummy flies ~17 m, still airborne at the end. Retuned to 180% (try 2).
- 180%: hit v91 (percent 180->191), actionState 3 Tumble v91-v342, flies ~22 m, lands v343 and is Actionable (0) at once: NO knockdown.
  Looked at the az90 sheet v330-v514: the dummy lands and stands. Cause not checked (defence rules of the dummy's kit: MaxDownedFrames / tech?).
- Walk into the STANDING dummy (v471-v504): centres go 94.9 -> 3.9 cm (v489), the dummy is moved only ~16 cm, the attacker walks straight
  through it (sheet v482-v498 shows the bodies overlapping). Pushbox on, but it does not stop a head-on walk. FINDING, not fixed.
- Dropkick 275 cm: HIT v89 (dummy Hitstun, 0->10%), attacker lands lying ~v112 with feet at the dummy's feet (~100 cm centres), gets up v140+,
  no shove, no slide, both standing ~100 cm apart at the end. Looked at az90 v80-v172 once.
- Files `docs/notes/reviews/pushbox-p3/`: dair_knockdown.mp4 (full, 60 fps), dair_hit_quarter_v80-v135, dair_walkthrough_quarter_v455-v515,
  dropkick_hit(+_quarter).mp4, az90 sheets.
## Open (for the owner via coordinator)
- Knockdown in game needs the dummy to stay downed after a tumble landing: check its defence rules (kit / match rules extension). Small thread.
- Head-on walk-through of a standing fighter: P1 soft capped push too weak (or degenerate head-on direction). Code thread if unwanted.
- Rule 10 cloud review of these videos.
**Cost**: estimated $3-4 (3 captures, no build).
