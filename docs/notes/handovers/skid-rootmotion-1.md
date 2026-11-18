# Skid follows the clip's root motion (owner choice A part 2) - branch claude/skid-rootmotion-1, worktree GASP_Giustino2_skidrm
Start: d871e7aa (claude/desktop-jab-fk). Code commit 78cc0004, review commit after it. No ABP change (owner: the ABP must stay readable).

## Done
- FGSTN_RunSkidTuning::RootTrack (GSTN_RunSkidTypes.h): baked samples (Speed signed along the entry direction, Turn 0..1, TurnSign)
  + tuning PlayRate, EntryBlendSeconds 0.1, ReverseSpeedScale 1, EndTime 0 (= clip end). Math in FGSTN_RunSkidMath::Track*.
- FGSTN_LayeredMove_RunSkid: with a usable track, every live frame takes speed + facing from the samples (clip time = frame*dt*PlayRate);
  RestFrame/ExitFrames come from the clip; the move ends at the clip end, the walking mode takes over. New serialized fields bClipTrack,
  StartFacing, TurnAngleDeg. Track off = old constant brake, unchanged.
- Bake: UGSTN_RunSkidTrackLibrary (BP: BakeRunSkidRootTrack, BakeProfileSkidRootTrack, SampleRunSkidRootTrack); profile field
  SkidRootMotionClip (auto bake on edit) + Bake Skid Root Track button. Tools/GAS/bake_skid_root_track.py (headless).
- DA_Locomotion_Default baked from SkidOwner/SS_180_Turn_Root180: 51 samples, 0.83 s, yaw +180, entry 90 cm/s, peak 277, rest 0.567 s,
  push-off only -60..-97 cm/s (Saved/Logs/ci/bake_skid_root_track.txt).
- Presentation: skid clip time runs at the track's PlayRate (GSTN_RunPresentationComponent::TickSkidPlayTime).

## Proofs
- Build: Saved/Logs/ci/skidrm_build1.log, `Select-String "Result: Succeeded"` = 1.
- Tests GSTNCombat: Saved/Logs/ci/skidrm_test1.log, 705 `Result={Success}`, 0 Fail/Error; new GSTNCombat.Core.RunSkid.RootTrack passes.
- Capture: pie_capture_timed.ps1 -Kit KIT_Test_Overlord -Out docs/notes/reviews/skid-rootmotion-1/cap -Sequences mv_skid_base (ESITO OK;
  cap/ not committed). actor_speed_f40-135.txt from frames.jsonl body.actor.

## Review (Fable, REVIEW-fable.md): FAIL, narrow
- PASS: one 180 turn; brake pose arrives with the brake (old FAIL fixed); feet locked in brake and turn (old 46 cm drag fixed).
- FAIL run restart: dati montage, f88-103 run pose while the capsule does ~84 cm/s (the clip's tail root, pelvis-derived, is too slow);
  gioco, walking takes over at f104 and reaches 450 by f109 under a planted foot (69 cm slide).
- To verify: f53 duplicate of f52 (harness or gioco).

## Next (attempt 2, recommended, rule 6): data only, no rebuild
- Re-bake the push-off/tail speed from the planted foot instead of the root bone (python AnimPose at time, speed = -foot velocity
  while a foot is planted), or hand over earlier (RootTrack.EndTime ~0.6 s). Needs one capture slot on _ue59 + one review.
- Owner design 2026-11-18 22:24Z (scope: run skid, stick released after the reversal): like Mario 3D World / Smash, play the brake +
  turn part of the clip, then stop facing the new direction, NO run restart; stick held = restart into the run as now. Not small
  (not done here): sim, in the clip-track branch, stick neutral once the clip's speed turns round (RestFrame) -> velocity 0, finish the
  turn to TurnDirection over a few frames, end the move; presentation: no clip tail when the stick is neutral, Skid state blends to Idle
  (the clip's root yaw is only 0.67 of the turn at rest, so the idle exit must cover the last ~60 deg). Code thread (Opus) + capture + review.
- Build before the next editor start from this worktree is done; _ue59 DLLs do not have this code until the branch is merged.

## Attempt 2 (coordinator go, 23:25-23:40 local) - review skid-rootmotion-2: FAIL narrow, BETTER than attempt 1. STOP (rule 5, 2 tries)
- Foot-based re-bake of the push-off (Tools/GAS/bake_skid_pushoff_from_foot.py): the clip's planted foot also moves ~73-91 cm/s
  (Saved/Logs/ci/bake_skid_pushoff.txt): the slow tail is the clip itself (dati montage). Not kept.
- Kept: presentation ends the skid clip at RootTrack.EndTime (GSTN_RunPresentationComponent::TickSkidPlayTime); EndTime = 0.7 s in
  DA_Locomotion_Default (Tools/GAS/set_skid_track_end_time.py). Build: Saved/Logs/ci/skidrm_build2.log, "Result: Succeeded" = 1.
- Review: turn and hand-over PASS (no mesh pop with the partial clip yaw); crawl 5 frames instead of 16; 450 cm/s at f100 not f109.
  FAIL (gioco): first two run steps skate (foot_l ~80 cm f98-110, foot_r 53 cm f110-120); minor side-step wobble f105-112.
- Next (code thread): what the locomotion graph plays when the Skid state exits (pose and blend), then stride warping / speed-matched
  play rate on the run blend-in, or a slower walking accel right after the skid. Or the owner re-times the clip tail (dati montage).
- Owner 2026-11-18 22:35Z: the original skid clip comes from the Mixamo Sword and Shield pack, which has NO root motion. The root in
  SS_180_Turn_Root180 was rebuilt from the pelvis (RTG / root_fix script), so its speed is an estimate: likely why the tail moves ~85 cm/s
  in a run pose. A next fix should not trust the clip root for the push-off (code: speed-matched run blend-in) or re-author the tail.
