# Handover: capsule on the ground, rolls, dropkick hop (thread "Sistemare capsula a terra e rotolate", 2026-11-24)

Branch `claude/project-thread-e8g12i` from main 5c9e05e5. Full notes: `docs/notes/greybox-capsula.md` (Desktop run 7, Laptop run 8);
videos `docs/notes/reviews/greybox-capsula/round7/` (before) and `round8/` (after).

What changed:
- Greybox capsule lies down with the driver's pelvis past a tilt (`LyingStartDeg`/`LyingFullDeg`), no ground squash, lean or sway while
  lying; floor lift keeps the posed capsule above the feet (`bKeepAboveFloor`, capsule geometry 34/14/156).
- Rolls (driver montage name contains `RollMontageNames`, AM_React_Roll / AM_React_RollBack): rigid somersault toward the travel from how it
  lay round to upright, timed from the latch (`RollTurnShare` 0.75).
- Self knockdown: the combat tick hands hooks `LandedTimeline` (the move the clock landed out of on a grounded frame); an armed Defence self
  knockdown takes that landing. Before, the dropkick's Landing window ended the move first and the hop never ran.
- Ground bounce: the knockback move's gravity scale is divided by the rules' KnockbackGravityScale (x4, 1A), so bounces keep
  GroundBounceGravityScale. Dropkick hop measured 28 cm, 17 frames (was 0, then 9 cm / 4 frames). Also restores victims' tech bounces.
- make_capsule_clips.py `MOVE_EXCLUDE` (TechBounce): no placeholder capsule clip for the tech bounce (it flipped lying/upright).
- pie_capture_move.py: gb_dk_* sequences, `PIE_CAPTURE_CAM_LOCK_Z`.

Open: dropkick air timeout (MaxAirFrames 125, owner 11-16) ends a high dropkick before touchdown, so no hop from a ~9 m jump; capsule dips
~20 cm for one frame on touchdown when the dropkick body capsule restores; forward roll fast at mid turn (RollTurnShare); registry entry
approved/runstart-v4r5 lists AS_GSTN_RunStart_v4r5 that the tag lacks (build run with -SkipRegress on the laptop).

ESITO: PASS built and tested (19/19 Greybox + Defence Landing) on desktop and laptop, round 8 seen in game; owner review in game pending.
SUBAGENTI: none (Remote Control on desktop then laptop, same model).
COSTO: not measured | ~40 min desktop + ~60 min laptop | 0 review tokens (capsule phase, no level-2 review)
