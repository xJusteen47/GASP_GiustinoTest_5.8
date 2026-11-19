# Crouch veloce, checkpoint 3 (desktop _ue59, Opus, 2026-11-19) - GROUND visible and in review, AIR stopped (2 attempts)
Branch `claude/crouch-veloce-2` (on bd75e014). Brief: coordinator note (owner "vai con tutti", item 1 of 3).

## Done
- `Tools/GAS/crouch/crouch_fk_build.py`: creates missing bone tracks before `set_bone_track_keys` (it silently wrote nothing on a
  missing track); logs track count (Saved/Crouch/crouch_fk_build.log, `grep "bone tracks = 93"` = 5 clips); plan path via env
  `CROUCH_PLAN` (project path has spaces); plan flag `tip_relative`: pitch/roll flipped for bones that HANG in the base pose.
- `crouch_plan_v2.json` (tip_relative, air pose more tucked). Ground clips AS_GSTN_Crouch_{Enter,Idle,Exit} rebuilt.
- `crouch_states.py`: Crouch Exit -> Idle Loop (was -> Conduit -> Idle Transition, which had no chooser row standing still, so the
  Blend Stack played the root fallback = skid clip; pose_match SS_180_Turn 7.6 cm after exit in Saved/Crouch/v2cap).
- `Tools/GAS/routing/clip_tracks_dump.py` (env CLIP_DUMP_OUT/CLIP_DUMP_CLIPS) for pose_match.py. Headless -script paths: forward
  slashes (a "\r" in "\routing" broke the load).
- Proof (Saved/Crouch/v3cap, `python -I Tools/GAS/routing/pose_match.py <cr_ground/frames.jsonl> Saved/Crouch/clip_tracks_v2.json <clip> <ref> a b`):
  Crouch_Idle v55-105 RMS 10.7 cm (GASP crouch 22.8); Exit v105-125 10.4 (stand 30.2); after exit stand idle 10.9 vs skid 11.4.
- Reviews: docs/notes/reviews/crouch-veloce-3/v3 (ground+air grids, mp4), v4 (air), frames/ (ground 38-52, 108-124),
  Fable verdict in review_fable_ground.md.

## Air crouch: FAIL posa, stopped (rule 5)
- v3 (tip flip on current direction) and v4 (flip on base pose) look identical in game: left foot forward ~ at pelvis height, the clip
  itself has foot_l 23 cm forward at z 72 (Saved/Crouch/clip_tracks_air.json). The plan's leg angles are wrong, not the sign code
  alone. Needs: author the tuck as target positions (knee and foot targets in character axes solved by the leg IK the ground clips
  already use) instead of raw thigh/calf angles; then one capture. Owner rule: air = air-time pose + crouch, more folded.
- AS_AirCrouch_Enter / _Loop_PLACEHOLDER are MODIFIED in the _ue59 working tree by the v2 build and NOT committed (git restore was
  refused by the permission classifier): restore them from HEAD before the playtest.

## State
- Lock _ue59 kept by this thread (next items: run start, skid/run stop review). Cost so far ~1 h, ~95k context.
