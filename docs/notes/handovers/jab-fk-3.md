# Handover: Overlord jab round 3 (desktop, 2026-11-18). Jab 1 Fable PASS (pose), jab 2 still PASS
Branch `claude/desktop-jab-fk`. Data 21a1d8c7 + 0d0ff716, review + variant in this commit. Review: `docs/notes/reviews/jab-fk-3/review.md`.

## What changed (plans `Tools/AnimPipeline/cards/overlord/jab/fk/plan_jab{1,2}_r3.json`, publish `fk/pub_jab_r3.py` headless)
- Owner note (rules 12/13a, overrides round-2 review item 1): in the owner's raw keys, key 1 has the LEFT foot back at (3.4,-22) and the
  RIGHT foot crossed in front of it at (26.4,5.1); key 7 lands it out right. Rounds 1-2 pinned both feet, so the crossover became a ground drag.
  r3: left foot steps back a0-a2 (lift 6), right foot crosses in front a2-a4, swings out and lands a9 (lift 8); returns staggered (right a17-a21,
  left a21-a25). No pelvis shift.
- Hand overshoot ~10 cm at a10 with wrist drag (hand_r local z -15 at a8, +8 at a10), settle a12, elbow breathing a15/a18.
- Start/end feet on the game idle spots: left (15.3,1.5), right (-14.7,-8.7) component cm (from faf_frames.jsonl f18). AM jab1 blend-in 0.05 s.
- Jab 2: f0 feet = jab 1 a11 pos+quat (chain joint error 0.0 cm, computed from the two *_bones.json), recovery left step to idle+18 (lift 7),
  right end on idle+18.

## Proof
- Publish: `pub_r3_result.txt` "jab1: 72/72 tracks", "ik gap a9 [0.0, 0.0]", "AM jab1 blend_in -> 0.050 s"; ESITO OK (pub_r3.log, 457 s).
- Capture: `MCP_PORT=8017 cap_one.sh KIT_Test_Overlord Saved/AnimR2/jabfk3/var_jab_r3 Attack` under Run-Timed, ESITO OK 467 s.
- Gioco offset checked by me in `variant/faf_frames.jsonl`: from idle, jab 1 a2 left foot (jsonl) (-34.9,-15.3); on the restart a2 (-22.0,-3.4) = plan.

## Open
- `gioco` (code queue): when jab 1 starts from idle, the feet sit ~13 cm back / 12 cm left of the authored spots and the left foot creeps ~15 cm
  over v23-v37; the restart after FAF is exact. Likely the runtime leg layer/foot IK leaving idle, not the sequence.
- Owner question: hand path is the same as r2 (only feet changed); if the slap still looks off to the right, turn the arm arc ~10 deg left.
- Polish (not blocking): crossover foot on the ground 1 frame (a4); jab 2 a29 both feet 3-4 cm up for one frame, palm contact 1 frame.
- Capture: montage time stuck 3 frames at jab 2 a11; az0 camera too close after the lunge.
Cost: about 120k context (Opus) + 1 Fable review subagent (~150k).
