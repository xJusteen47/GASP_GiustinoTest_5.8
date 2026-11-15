# CAPSULE-MOVES-1 handover: Body Capsule data for dair v9; knockdown and crouch need decisions (desktop, 2026-11-15). PARTIAL
Branch `claude/integ-dropkick-capsule` (on a95e17d4), folder `GASP_Giustino2_ue59`. Opus, no subagents, no build, no C++ change. Not pushed.
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. Only `TL_Overlord_DAirV9` saved. No pose, key, map or project setting.
## Scope found
- dair v9: has a timeline -> data. `Tools/GAS/setup_dair_body_capsule.py` (run after the dair kit script) measures the clip per frame and writes
  Body Capsule tracks (Body anchor, 20 cm/frame): capsule from the lowest foot - 8 cm (never under the mesh root) to head + 15, forward on the
  torso, no sideways offset; bottom frozen after the deepest foot frame. 10 tracks, log `docs/notes/reviews/capsule-moves-dair/setup_dair_body_capsule.txt`.
- Measured (same log): the dair is UPRIGHT with the feet down (torso z 77-110, feet to -9), not horizontal: the standing capsule is right in
  height; the body is 8-17 cm forward and 10 cm sideways of it. Tucked startup (f0-12): capsule hh 31-47 around the ball.
- Knockdown/ground reactions: the modifier reads only `ReadEvaluatedTimeline` (null in a recoil); downed/tumble state is the defence fragment
  (`KnockdownEndFrame`), no timeline -> NOT data-only, needs a code hook. Crouch attacks: none exist (only Jab1/2, DAir V7-9, Dropkick timelines).
## Proof (captures: Run-Timed -Kind Capture, map L_Test_Hub, KIT_Test_OverlordDAirV9, PIE_CAPTURE_COLLISION=1, seq dair_land,dair_hit; all ESITO OK)
- Files `docs/notes/reviews/capsule-moves-dair/`: mp4 base/var (hit, land), var quarter (hit, land), side sheets. Looked at: hit az90/az180, land az90.
- Landing (`python - ` over `Saved/CapsuleMoves/{base,var}/dair_land/frames.jsonl`, body.pelvis_z v94-v149): same as base within 2 cm, rest 92 both.
- Pops: `python Tools/GASNPPTest/body_jumps.py Saved/CapsuleMoves/var` (pelvis jumps > 14 cm/frame): only fall-speed frames, as base. Attempt 1
  had a -17 cm end pop (bottom under the root, v122), fixed by the root clamp.
- Hit: body no longer enters the victim (sheets hit_base vs hit_var v92-v125). BUT the attacker is pushed back ~65-75 cm after the hit
  (frames.jsonl `loc[0]` 9 -> -65, base stays 0): the forward capsule overlaps the victim's (62 cm apart, radii 30) and slides off it.
  Attempt 2 (sideways offset off) did not change it. Stopped (rule 5).
## Decisions for the owner (coordinator)
1 dair push-back: A keep (no overlap, attacker ends ~70 cm away) / B forward offset off too (= base spacing, body enters again) /
  C recommended: radius 22-24 on the dair tracks, one capture, ~$3.
2 knockdown/lying victims: A recommended: code thread (Opus medium ~$10-12): the modifier also takes a capsule from the defence downed/tumble
  state, data per reaction (lying: hh = radius, on the torso) / B later.
3 crouch attacks: nothing to do until they exist; their kit script calls the same fitting (generalise the dair script then).
**Cost**: estimated $8-10 (one Opus thread ~95k context, 4 captures, no build).
