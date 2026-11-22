# Run start frantic 2 + crouch r2 + skid/run stop review (desktop _ue59, Opus, 2026-11-19) - all FAIL, nothing to the owner
Branch `claude/crouch-veloce-2` 2140452f (= crouch 8478060a + merge of claude/project-thread-96rsau af31cb56). Brief: coordinator note,
owner "vai con tutti" (items 2 and 3; item 1 = handover crouch-veloce-3).

## Run start (integration attempt 1)
- Merge conflict in GSTN_RunPresentationComponent.h resolved (both blocks kept). Build OK (Saved/RunStart/build1.log, `grep -c "Result: Succeeded"` = 1).
  Tests GSTNCombat.Core.RunStages + GSTNCombat.NP: 230/230 (Saved/RunStart/test1.log, Check-TestLog "ESITO TEST: PASS").
- setup_run_start_stamp.py: stamp on in DA_Locomotion_Default (21 f x0.05). `Tools/GAS/run_start/run_start_states.py`: imports r3 FBX as
  /Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Frantic (0.867 s, 91 tracks), state "Run Start" + alias "-> Run Start" (Idle Loop/Transition/Break,
  GetAnimOwnerRunStartTime >= 0), exit to Conduit (< 0 or >= length - 0.1); route_states.py row "Run Start" (Saved/Routing/route_states.txt).
- Capture Saved/RunStart/cap1 (rs_start, new sequence in pie_capture_move.py). Fable: FAIL gioco, clip NOT on screen
  (docs/notes/reviews/run-start-frantic/ingame1/review_fable.md): idle during the stamp, then the GASP start. Pose match clip 17.2 vs idle 11.9 cm.
  `grep -c "LocoState enter state=Run Start" Saved/RunStart/cap1/run.log.ue.log` = 1 entry only. Next: log GetAnimOwnerRunStartTime per frame
  (is it >= 0 from the push or only later/briefly?), check the state leaves at once (exit rule), and that MM/Locomotion Transition does not win
  (alias only from idle states; the stamp holds speed ~0 so MovementState may flip to moving first). L3 take not captured (no boost hook in the tool).

## Crouch ground round 2 (plan v3): FAIL narrow (review_fable_ground_r2.md)
- Timing and follow-through PASS. Style still a one-knee kneel: back knee 10-12 cm off the floor + back heel up, chin fist 12-15 cm in front,
  elbows out (upperarm roll ~35), head 6-8 deg more up, check lead fist side; exit rise pops (80% in one frame f112-114): spread over 4 frames.
  Round 3 is the last before the owner decides (rule 10). Capture camera snap with the capsule = gioco (code queue).

## Skid / run stop (Saved/Review3/cap1, docs/notes/reviews/skid-runstop-3/review_fable.md, gstn.RunStop.Mode 1)
- SKID FAIL gioco: brake good, but body faces the old direction f134-188 then snaps (double-turn), feet slide ~95 cm, clip end held.
- RUN STOP FAIL narrow: posa (stands tall too soon after the lunge, pelvis bump f190-194), gioco (no dust on the plant), capture too short for the settle.

## State left
- _ue59 on claude/crouch-veloce-2 2140452f; AirCrouch clips MODIFIED and uncommitted (restore from HEAD before the playtest). Lock released.
- Cost: ~2 h desktop, Opus, ~150k context, 4 Fable review subagents (~370k tokens).
