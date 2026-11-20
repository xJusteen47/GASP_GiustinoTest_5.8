# ANIM-JAB-3: a REAL Play capture of a kit's move for the animation review (project rule 10: someone watches the in-game video before the owner).
# Unlike Tools/AnimPipeline/ingame_judge.py (a montage scrubbed in L_PilotCapture, no game running), this starts Play in L_Test_Hub (listen host
# + 2 clients, the hub's own automation), gives the host the kit, presses the kit's attack button through the combat component's scripted
# buttons (resolved by name from the input config, no key is named here) and shoots every rendered frame from fixed review cameras plus the
# player's own camera. While a sequence runs the engine steps exactly 1/60 s per rendered frame (t.OverrideFPS 60), so a slow capture drops no
# frame: one image = one 60 fps frame of what a player sees. The simulation rate is not touched.
#
# Launch (editor with a real renderer; the script path must have no space, so boot through a copy or a one-line exec):
#   UnrealEditor.exe <uproject> /Game/Levels/Test/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/pie_capture_move.py").read())
# Environment (all optional): PIE_CAPTURE_KIT (KIT_Test_Overlord), PIE_CAPTURE_OUT (default Saved/AnimJab3/capture_<date>_<time>),
# PIE_CAPTURE_SEQUENCES (default: all of SEQUENCES, or all of DAIR_SEQUENCES when the kit's name contains DAir)
# Down-air captures (ANIM-DAIR-1): PIE_CAPTURE_KIT=KIT_Test_OverlordDAirV8 (or ...V7, one kit per Play session) and PIE_CAPTURE_OUT=<dir>; the
# sequences with an 'air' entry jump first and press crouch + attack on the way down, see DAIR_SEQUENCES
# Review stage, this Play session only: the other players are parked in Z1, the kit's placeholder cube is hidden and three shadowless
# directional fill lights shine on the pawn (the hub's sun is behind the player start, the character's front would be black).
# Output: <out>/<sequence>/<view>/f<frame>.png, <out>/<sequence>/frames.jsonl (the host's combat state per frame), <out>/capture.log and
# <out>/done.txt ("ok" or the error). Nothing is saved: the level and the kits stay as they are on disk. Then QUIT_EDITOR.
# The grids and the mp4 are made outside the editor by Tools/GASNPPTest/pie_capture_grid.py.
# ASSERT (project rule 10): sequences with an 'expect' entry, or PIE_CAPTURE_ASSERT=<asserts.json>, are checked at the end by
# Tools/AnimPipeline/capture_assert.py; <out>/assert.txt starts with ASSERT: OK or ASSERT: FAIL. A review starts only on OK.
import json
import math
import os
import sys
import time
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g   # noqa: E402
import test_hub as h      # noqa: E402

KIT = os.environ.get('PIE_CAPTURE_KIT', 'KIT_Test_Overlord')
OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimJab3',
                                                        time.strftime('capture_%Y%m%d_%H%M%S'))
SIZE = 480
# Review cameras around the pawn: (name, azimuth in degrees from the pawn's forward, as a yaw: 0 = in front of it, 90 = on its RIGHT, 270 = on
# its left, 45 = front right, 315 = front left). 'game' = the player's camera
VIEWS = [('az0', 0.0), ('az45', 45.0), ('az90', 90.0), ('az180', 180.0), ('az270', 270.0), ('az315', 315.0), ('game', None)]
ELEVATION, DISTANCE, FOV = 10.0, 380.0, 45.0
# Close framing for small things (a 15 cm projectile): PIE_CAPTURE_DIST (cm, default 380) and PIE_CAPTURE_AHEAD (cm the look-at point is moved in front of the pawn, default 0); the camera is scaled, never the projectile
DISTANCE = float(os.environ.get('PIE_CAPTURE_DIST', DISTANCE))
AHEAD = float(os.environ.get('PIE_CAPTURE_AHEAD', 0.0))
FILL_LIGHTS = [(35.0, 5.0), (325.0, 5.0), (95.0, 3.0)]   # (azimuth the light comes from, as in VIEWS; intensity in lux)
PRE = 20         # idle frames shot before the first press (the captures' temporal history settles)
# The button every tap presses, by its name in the input config: PIE_CAPTURE_BUTTON (default Attack; Special for the double-shot kits). It was
# documented but never read until 2026-11-15, so every capture pressed Attack (the CP5 double-shot captures ran the base kit's light attack)
BUTTON = os.environ.get('PIE_CAPTURE_BUTTON', 'Attack')
# taps: rendered frames after PRE on which the button is pressed for one frame. dummy: a dummy this many cm ahead (None = whiff)
SEQUENCES = [
    {'name': 'jab1', 'taps': [0], 'frames': 75, 'dummy': None, 'shoot': True},
    # STANCE-OFFSET: the same jab, long enough to see the upper-body stance come back after the feet step (~frame 106 with the default timing)
    {'name': 'jab1_long', 'taps': [0], 'frames': 140, 'dummy': None, 'shoot': True},
    {'name': 'chain', 'taps': [0, 12], 'frames': 120, 'dummy': None, 'shoot': True},
    {'name': 'chain_buffered', 'taps': [0, 4], 'frames': 120, 'dummy': None, 'shoot': False},
    {'name': 'chain_hit', 'taps': [0, 12], 'frames': 140, 'dummy': 95.0, 'shoot': True},
    # Same as chain / chain_hit under the names of the review notes. jab1_jab2_faf: a third press at the first actionable frame of jab 2
    # (timeline 26; jab 2 starts on the video frame after the press, so 12 + 1 + 26 = tap 38 puts the new jab 1 at timeline 26 of jab 2)
    {'name': 'jab1_jab2', 'taps': [0, 12], 'frames': 120, 'dummy': None, 'shoot': True},
    {'name': 'jab1_jab2_hit', 'taps': [0, 12], 'frames': 140, 'dummy': 95.0, 'shoot': True},
    {'name': 'jab1_jab2_faf', 'taps': [0, 12, 38], 'frames': 130, 'dummy': None, 'shoot': True},
]
# PIE_CAPTURE_SEQUENCES: a comma separated subset of the names above (a recheck of one case does not pay for the other captures)
# Down-air (aerial) sequences. 'air': the pawn jumps on frame PRE (a press of Jump) and, on the way down (z falling) once its height above the
# standing height is at most trigger_h cm, crouch is held and, the next rendered frame, Attack is tapped (command CrouchNeutral, bound while
# Airborne by setup_overlord_dair_kit.py). The press frame is the same for the V8 and the V7 kit (the simulation is deterministic), so frame
# numbers compare; if the height is never reached the press is made at PRE + AIR_GIVE_UP and the log says so. trigger_h is a first guess for the
# jump height of the test character (not measured, the rows hold 'h' per frame to retune it): high = the move finishes in the air, low = the
# character lands in the middle of it. Order matters: the hit case comes last because its dummy stays where it was put.
# Measured on the first run (2026-11-12, capture_v8_r2): the test character's jump peaks 233 cm above the standing height 42 frames after the press
# and lands 37 frames after the peak, so a 43 frame move pressed on the way down always lands before its end (pressed at 136 cm it landed on
# animation frame 12, before the strike). Hence: the whiff is pressed on the way UP ('rise_h': the first rising frame at or above that height) and
# ends in the air; the landing case is pressed at 184 cm falling (lands about frame 20, in the hold); the hit case at 225 cm falling, so the feet are
# at about 150 cm (the dummy's chest and head) on frame 15, with the dummy 62 cm ahead (the hitbox is a 40 cm sphere on the foot, a pawn is about
# 30 cm wide: at 80 cm it did not reach). 'jump_only' never presses: it records where a plain jump lands (the standing height after a landing)
DAIR_SEQUENCES = [
    {'name': 'dair_whiff', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'air': {'rise_h': 150.0}},
    {'name': 'dair_land', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'air': {'trigger_h': 184.0}},
    {'name': 'jump_only', 'taps': [], 'frames': 110, 'dummy': None, 'shoot': False, 'air': {}},
    {'name': 'dair_hit', 'taps': [], 'frames': 130, 'dummy': 62.0, 'shoot': True, 'air': {'trigger_h': 225.0}},
    # DAIR-V10 (2026-11-17): with the current jump dair_hit lands on anim ~10, before the strike (with V10's Landing windows the move ends in the
    # heavy landing lag, no hit). dair_hit_rise presses on the way up as the whiff, so the strike happens in the air; dummy PIE_CAPTURE_DAIR_HIT_DIST
    # cm ahead (default 40)
    {'name': 'dair_hit_rise', 'taps': [], 'frames': 130, 'dummy': float(os.environ.get('PIE_CAPTURE_DAIR_HIT_DIST', 40.0)), 'shoot': True,
     'air': {'rise_h': float(os.environ.get('PIE_CAPTURE_DAIR_HIT_RISE', 100.0))}},
    # DAIR-V10 review r1: a landing in the move's first frames (pressed falling just over the floor: light window f1-3) and a late one (pressed
    # rising lower than the whiff: the hang ends near the floor, landing in the last frames: light window f32+)
    {'name': 'dair_land_early', 'taps': [], 'frames': 110, 'dummy': None, 'shoot': True,
     'air': {'trigger_h': float(os.environ.get('PIE_CAPTURE_DAIR_EARLY_H', 30.0))}},
    {'name': 'dair_land_late', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True,
     'air': {'rise_h': float(os.environ.get('PIE_CAPTURE_DAIR_LATE_RISE', 60.0))}},
    # review r2: try for the late light window from the apex (pressed falling just under the peak, the longest fall the hub's jump gives)
    {'name': 'dair_land_apex', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True,
     'air': {'trigger_h': float(os.environ.get('PIE_CAPTURE_DAIR_APEX_H', 240.0))}},
    # DEBUG-HUB (owner 2026-11-16, "does the stomp really take the enemies below, or do pushbox and core stop it?"): the dummy almost under the
    # attacker (PIE_CAPTURE_BELOW_DIST cm ahead, default 20), same jump and press; run with gstn.Combat.Debug.Hitboxes/Body on (PIE_CAPTURE_CVARS)
    {'name': 'dair_below', 'taps': [], 'frames': 130, 'dummy': float(os.environ.get('PIE_CAPTURE_BELOW_DIST', 20.0)), 'shoot': True,
     'air': {'trigger_h': 225.0}},
    # PUSHBOX-P3: the same hit on a dummy at a high percent (test-only server write of its DamagePercent, PIE_CAPTURE_DUMMY_PERCENT, default 180: at 150 the launch, ~1700 cm/s up and forward, stayed under the 1800 cm/s tumble threshold and flew ~17 m)
    # so the landing hit tumbles it into a knockdown; then the stick is held forward ('walk': from this many frames after the press, for this many
    # frames) so the attacker walks over the downed body (no pushbox expected while it is down)
    {'name': 'dair_knockdown', 'taps': [], 'frames': 520, 'dummy': 62.0, 'shoot': True, 'air': {'trigger_h': 225.0},
     'dummy_percent': float(os.environ.get('PIE_CAPTURE_DUMMY_PERCENT', 180.0)), 'walk': (50, 400)},
    # NIGHT-KD-1: the same hit, then the attacker walks up to ~3 m from where the dummy lands (~22 m at 180%) and stops, so the side cameras
    # frame the dummy lying on the floor (the 520-frame run ends with the dummy 10 m off camera)
    {'name': 'dair_knockdown_near', 'taps': [], 'frames': 760, 'dummy': 62.0, 'shoot': True, 'air': {'trigger_h': 225.0},
     'dummy_percent': float(os.environ.get('PIE_CAPTURE_DUMMY_PERCENT', 180.0)), 'walk': (50, 480)},
]
# Dropkick (forward air, ANIM-R2): same 'air' mechanism, but the stick is held forward (instead of crouch) and Attack tapped the next frame
# (command Directional while Airborne, setup_overlord_dropkick_kit.py): PIE_CAPTURE_KIT=KIT_Test_OverlordDropkick. The move is 80 frames long
DROPKICK_SEQUENCES = [
    # F4 (owner 2026-11-17, orange Overlord kit): after the forward air (dropkick) lands, the stick is pushed forward: the character must run, not slide
    {'name': 'dropkick_land_walk', 'taps': [], 'frames': 220, 'dummy': None, 'shoot': True, 'air': {'rise_h': 150.0, 'stick': True, 'press_delay': 5},
     'sticks': [(int(os.environ.get('PIE_CAPTURE_LAND_WALK_AT', 130)), (0, 1, 0)), (210, None)]},
    {'name': 'dropkick_whiff', 'taps': [], 'frames': 150, 'dummy': None, 'shoot': True, 'air': {'rise_h': 150.0, 'stick': True, 'press_delay': 5}},
    # PUSHBOX-P2: the same press with a dummy ahead, so the landing and the end meet a body. P3: 120 cm let the kick pass ~230 cm over the dummy
    # (no hit), the kick connects further out: PIE_CAPTURE_DROPKICK_DIST (default 275 cm)
    {'name': 'dropkick_hit', 'taps': [], 'frames': 150, 'dummy': float(os.environ.get('PIE_CAPTURE_DROPKICK_DIST', 275.0)), 'shoot': True, 'air': {'rise_h': 150.0, 'stick': True, 'press_delay': 5}},
    # DROPKICK-BOUNCE-1: a LOW jump case for the user's landing hop: pressed on the way down at PIE_CAPTURE_DROPKICK_LOW_H cm (default 90), so
    # the kick lands from a short fall; the hit variant puts the dummy PIE_CAPTURE_DROPKICK_LOW_DIST cm ahead (default 150)
    {'name': 'dropkick_low_whiff', 'taps': [], 'frames': 150, 'dummy': None, 'shoot': True,
     'air': {'trigger_h': float(os.environ.get('PIE_CAPTURE_DROPKICK_LOW_H', 90.0)), 'stick': True}},
    {'name': 'dropkick_low_hit', 'taps': [], 'frames': 150, 'dummy': float(os.environ.get('PIE_CAPTURE_DROPKICK_LOW_DIST', 150.0)), 'shoot': True,
     'air': {'trigger_h': float(os.environ.get('PIE_CAPTURE_DROPKICK_LOW_H', 90.0)), 'stick': True}},
]
# CP3 double-shot examples (PIE_CAPTURE_KIT=KIT_Example_DoubleShotA / B, PIE_CAPTURE_BUTTON=Special): one press, two presses 8 frames apart (a double
# press), two presses 40 frames apart (two single shots)
DOUBLE_SHOT_SEQUENCES = [
    {'name': 'shot_single', 'taps': [0], 'frames': 90, 'dummy': None, 'shoot': True},
    {'name': 'shot_double', 'taps': [0, 8], 'frames': 90, 'dummy': None, 'shoot': True},
    {'name': 'shot_slow', 'taps': [0, 40], 'frames': 110, 'dummy': None, 'shoot': True},
]
# CP4 hit cue + hitstop shake (PIE_CAPTURE_KIT=KIT_Example, PIE_CAPTURE_BUTTON=Attack): one light attack on a dummy 95 cm ahead
CUE_SEQUENCES = [{'name': 'cue_hit', 'taps': [0], 'frames': 90, 'dummy': 95.0, 'shoot': True}]
# PUSHBOX-P4 (rigid core): walk_into holds the stick forward from PRE into a standing dummy 200 cm ahead (P3 walked straight through it);
# head_landing jumps at PRE with the stick forward for the first 'walk' frames toward a dummy PIE_CAPTURE_HEAD_DIST cm ahead (lands on or by its head)
PUSHBOX_SEQUENCES = [
    {'name': 'walk_into', 'taps': [], 'frames': 220, 'dummy': 200.0, 'shoot': True, 'walk': (0, 200)},
    {'name': 'head_landing', 'taps': [], 'frames': 160, 'dummy': float(os.environ.get('PIE_CAPTURE_HEAD_DIST', 150.0)), 'shoot': True, 'air': {},
     'walk': (0, int(os.environ.get('PIE_CAPTURE_HEAD_STICK', 40)))},
]
# PUSHBOX-P5 (minion core + pushbox): 'minions': (count, bench scenario) spawned by gstn.Minion.Bench.Spawn in a 600 cm ring around the host at
# the start of the sequence (cleared first). minion_chase: the fighter stands while they run at it (fighter vs minion cores, minions vs each
# other); minion_walk: the same crowd, then the stick is held forward into it. Rows carry the host world's puppet positions ('minions')
MINION_SEQUENCES = [
    {'name': 'minion_chase', 'taps': [], 'frames': 260, 'dummy': None, 'shoot': True, 'minions': (8, 'Chase')},
    {'name': 'minion_walk', 'taps': [], 'frames': 260, 'dummy': None, 'shoot': True, 'minions': (8, 'Chase'), 'walk': (90, 160)},
]
# PUSHBOX-P5, owner PIE bug 2026-11-16 (run with PIE_CAPTURE_BUTTON=Strong): strong_walk hits a dummy 95 cm ahead with the strong attack, then
# walks at it (the owner stopped ~2 m short for seconds); inside_escape puts the dummy 10 cm ahead (bodies overlapping) and walks away
# ('walk_dir': the stick, default forward). Log the constraint with gstn.Pushbox.Log 1 (PIE_CAPTURE_PUSHLOG=1)
BUG_SEQUENCES = [
    {'name': 'strong_walk', 'taps': [0], 'frames': 300, 'dummy': 95.0, 'shoot': True, 'walk': (40, 240)},
    {'name': 'inside_escape', 'taps': [], 'frames': 160, 'dummy': 10.0, 'shoot': True, 'walk': (20, 120), 'walk_dir': (0, -1, 0)},
]
AIR_GIVE_UP, AIR_CROUCH_RELEASE = 100, 4   # frames after the jump press / after the attack press
# Movement QA clips (qa-movement-1117): no attack. 'sticks' = [(frame from PRE, IA_Move value or None = release)], camera relative ((0, 1) = away
# from the game camera); 'jump' = (frame, frames held) through the combat Jump button; 'crouch_tap' = frame of a Crouch tap (fast fall);
# 'follow' = the review cameras move with the pawn at the start yaw. Select with PIE_CAPTURE_SEQUENCES (the names below), and
# PIE_CAPTURE_VIEWS=az90,az0,game keeps fewer views (one grid per view, project rule 4)
MOVE_SEQUENCES = [
    # Slopes (owner 2026-11-19): with PIE_CAPTURE_PARK=Z7:1 (foot of the 15 deg hill) or Z7_top:1 (its plateau). The pawn spawns facing +X;
    # the hills rise toward -X, so (0, -1) runs uphill from the foot and (0, 1) runs downhill from the plateau
    {'name': 'mv_slope_up_stop', 'taps': [], 'frames': 240, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, -1, 0)), (120, None)]},
    {'name': 'mv_slope_up_skid', 'taps': [], 'frames': 240, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, -1, 0)), (100, (0, 1, 0)), (140, None)]},
    {'name': 'mv_slope_down_stop', 'taps': [], 'frames': 200, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (75, None)]},
    {'name': 'mv_slope_down_skid', 'taps': [], 'frames': 200, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (55, (0, -1, 0)), (95, None)]},
    {'name': 'mv_slope_idle', 'taps': [], 'frames': 90, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': []},
    # Run stop at the BASE run stage (owner 2026-11-18, Ganondorf-style stop): stick let go at stage 1, 100 frames to see brake + settle into idle
    {'name': 'mv_run_stop_base', 'taps': [], 'frames': 135, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (35, None)]},
    # run stop idle (locomotion-finish-4): 150+ frames of rest after the clip, to see which idle the game settles in (combat.anim)
    {'name': 'mv_run_stop_rest', 'taps': [], 'frames': 420, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (160, None)]},
    {'name': 'mv_run_stop', 'taps': [], 'frames': 260, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (160, None)]},
    # run start (idle -> run forward): 1 s idle, full stick forward, held 3 s (60 fps frames); stock check: PIE_CAPTURE_CVARS="gstn.Loco.Base 1"
    {'name': 'mv_run_start', 'taps': [], 'frames': 260, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(60, (0, 1, 0))]},
    {'name': 'mv_skid', 'taps': [], 'frames': 200, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (95, (0, -1, 0)), (170, None)]},
    # Skid at the BASE run stage (MinStage 0, 11-18): reverse before FramesToNextStage (40 sim f) so the run is still stage 1 (470 > MinSpeed 350)
    # skid-finish-4: released early (30 frames after the reverse, inside Skid Exit Run) and held long (1.5 s)
    # dash-ganon: L3 boost from idle with the stick pushed on the same frame (Smash dash); 'boost' = frame of a RunBoost tap
    # the boost goes 3 frames after the stick: the injected stick reaches the sim one frame after a press (dash cap1/cap2 history
    # "+J" one frame before "8"), and a boost seen with the stick at rest is dropped by the stick-release branch of the run stages
    {'name': 'mv_dash', 'taps': [], 'frames': 120, 'dummy': None, 'shoot': True, 'follow': True, 'boost': 23, 'sticks': [(20, (0, 1, 0)), (80, None)]},
    {'name': 'mv_dash_base', 'taps': [], 'frames': 120, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(20, (0, 1, 0)), (80, None)]},
    {'name': 'mv_skid_release', 'taps': [], 'frames': 190, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (95, (0, -1, 0)), (125, None)]},
    {'name': 'mv_skid_hold', 'taps': [], 'frames': 280, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (95, (0, -1, 0)), (245, None)]},
    {'name': 'mv_skid_base', 'taps': [], 'frames': 150, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 1, 0)), (30, (0, -1, 0)), (110, None)]},
    {'name': 'mv_walk_low', 'taps': [], 'frames': 140, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(0, (0, 0.2, 0)), (110, None)]},
    {'name': 'mv_jump_full', 'taps': [], 'frames': 90, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (0, 24)},
    {'name': 'mv_jump_tap', 'taps': [], 'frames': 70, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (0, 0)},
    # Phase 3 batch 2 (jump squat + land): settled idle, full jump, then 110 frames of rest to see the land and the return to idle;
    # mv_run_jump: the same jump from a run (land while moving), stick released after the land to see the stop too
    {'name': 'mv_jump_rest', 'taps': [], 'frames': 190, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (20, 24)},
    {'name': 'mv_run_jump', 'taps': [], 'frames': 220, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (70, 24),
     'sticks': [(0, (0, 1, 0)), (150, None)]},
    {'name': 'mv_air_reverse', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (55, 18),
     'sticks': [(0, (0, 1, 0)), (62, (0, -1, 0)), (115, None)]},
    # M1 depth marker clips (rule 10): 'marker' turns gstn.DepthMarker.Enable on; 'proj_volume' spawns a projection box around the start (owner-local sideways)
    {'name': 'dm_circle_jump', 'taps': [], 'frames': 300, 'dummy': None, 'shoot': True, 'follow': True, 'marker': True, 'jump': (105, 20),
     'sticks': [(0, (0, 1, 0)), (25, (0.7, 0.7, 0)), (50, (1, 0, 0)), (75, (0.7, -0.7, 0)), (100, (0, -1, 0)), (125, (-0.7, -0.7, 0)),
                (150, (-1, 0, 0)), (175, (-0.7, 0.7, 0)), (200, (0, 1, 0)), (240, None)]},
    {'name': 'dm_projection', 'taps': [], 'frames': 160, 'dummy': None, 'shoot': True, 'follow': True, 'marker': True, 'proj_volume': True,
     'sticks': [(0, (1, 0, 0)), (60, (-1, 0, 0)), (120, None)]},
    {'name': 'mv_fast_fall', 'taps': [], 'frames': 80, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (0, 24), 'crouch_tap': 36},
    # Fast crouch (crouch-veloce-2): 'crouch_hold' = (frame from PRE, frames held) of the combat Crouch button. Ground in/out, air crouch
    # released mid-air, and landing with crouch still held
    {'name': 'cr_ground', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'follow': True, 'crouch_hold': (20, 70)},
    {'name': 'cr_air', 'taps': [], 'frames': 100, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (0, 24), 'crouch_hold': (30, 20)},
    {'name': 'cr_land', 'taps': [], 'frames': 150, 'dummy': None, 'shoot': True, 'follow': True, 'jump': (0, 24), 'crouch_hold': (30, 90)},
    # Frantic run start (run-start-frantic-2): full stick from still after 20 idle frames, run 70 frames, release
    {'name': 'rs_start', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'follow': True, 'sticks': [(20, (0, 1, 0)), (90, None)]},
]
if os.environ.get('PIE_CAPTURE_VIEWS'):
    VIEWS = [v for v in VIEWS if v[0] in os.environ['PIE_CAPTURE_VIEWS'].split(',')]
if os.environ.get('PIE_CAPTURE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES + MOVE_SEQUENCES + DAIR_SEQUENCES + DROPKICK_SEQUENCES + DOUBLE_SHOT_SEQUENCES + CUE_SEQUENCES + PUSHBOX_SEQUENCES + MINION_SEQUENCES + BUG_SEQUENCES if s['name'] in os.environ['PIE_CAPTURE_SEQUENCES'].split(',')]
elif 'DoubleShot' in KIT:
    SEQUENCES = DOUBLE_SHOT_SEQUENCES
elif 'Dropkick' in KIT:
    SEQUENCES = DROPKICK_SEQUENCES
elif 'DAir' in KIT:
    SEQUENCES = DAIR_SEQUENCES
TIMEOUT = 900.0
# PIE_CAPTURE_COLLISION=1: draw collision (the pawn's movement capsule) in every view and the game viewport (DROPKICK-CAPSULE, 2026-11-15)
COLLISION = os.environ.get('PIE_CAPTURE_COLLISION', '') == '1'
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'cams': [], 'rts': [], 'lights': [], 'handle': None, 'log': None, 'rows': None}


def _log(msg):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'capture.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {msg}\n')


def _world():
    return g.worlds()[0]


def _pawn():
    return g._local_pawn(0)[1]


def _console(cmd):
    unreal.SystemLibrary.execute_console_command(_world(), cmd, g._local_pawn(0)[0])


def _ready():
    ws = g.worlds()
    if len(ws) < int(os.environ.get('PIE_CAPTURE_CLIENTS', '2')) + 1 or _pawn() is None:
        return False
    comp = g.local_comp(ws[0])
    return bool(comp and json.loads(comp.get_state_snapshot()).get('inputReady'))


def _make_cameras():
    """Scene captures cannot be spawned in a Play world from Python: the cheat manager's Summon does it (one per call, found on the next tick)"""
    if st['cams']:
        return True
    found = list(unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.SceneCapture2D))
    if len(found) < len(VIEWS):
        _console('Summon SceneCapture2D')
        return False
    st['cams'] = found[:len(VIEWS)]
    for cam in st['cams']:
        comp = cam.capture_component2d
        rt = unreal.RenderingLibrary.create_render_target2d(_world(), SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        st['rts'].append(rt)
        for prop, value in (('capture_every_frame', False), ('capture_on_movement', False), ('always_persist_rendering_state', True),
                            ('fov_angle', FOV), ('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR), ('texture_target', rt)):
            comp.set_editor_property(prop, value)
        if COLLISION:   # the movement capsule drawn in every view (ECB review): the scene captures have their own show flags
            comp.set_editor_property('show_flag_settings', [unreal.EngineShowFlagsSetting(show_flag_name='Collision', enabled=True)])
    if COLLISION:
        _console('show collision')
    return True


def _clear_stage():
    """Review only, nothing saved: the other players leave the piazza (they spawn a step away and hide the move) and the kit's placeholder cube
    on the back is hidden (it covers the trunk and the arms from behind)"""
    parked = [h.park(i, 'Z1') for i in h._players() if i != 0]
    me = _pawn().get_actor_location()
    # F4 (2026-11-17): the clients' worlds often have no possessed pawn yet (h._players() = host only), so their pawns stayed in the piazza
    # (one 48 cm from ours in the dair capture). Park any other player pawn (key P<n>) found on the server within 1000 cm, by its comp
    if not parked:
        spots = h.hub.PARK['Z1']
        for n, (key, comp) in enumerate(sorted(g.comps(g.worlds()[0]).items())):
            owner = comp.get_owner()
            if owner == _pawn() or not str(key).startswith('P') or owner.get_actor_location().distance(me) > 1000:
                continue
            x, y = spots[n % len(spots)]
            spawn = comp.get_spawn_location()
            comp.server_reset_to_spawn(unreal.Vector(x - spawn.x, y - spawn.y, 0.0))
            parked.append((key, (x, y)))
    _console('gstn.Combat.KitPlaceholderLook 0')
    others = [round(c.get_owner().get_actor_location().distance(me)) for c in g.comps(_world()).values() if c.get_owner() != _pawn()]
    return f'parked {parked}, other pawns at {sorted(others)[:4]} cm'


def _make_lights():
    """The hub's only light is a sun behind the player start and it has no sky: the front of the character is black (point lights do not help,
    the mannequin is metallic). Shadowless directional fill lights (summoned like the cameras, aimed with them) make every side readable;
    they exist only in this Play session"""
    if st['lights']:
        return True
    if st.get('lights_before') is None:
        st['lights_before'] = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.DirectionalLight)}
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.DirectionalLight) if a.get_name() not in st['lights_before']]
    if len(new) < len(FILL_LIGHTS):
        _console('Summon DirectionalLight')
        return False
    st['lights'] = new[:len(FILL_LIGHTS)]
    for (_az, intensity), light in zip(FILL_LIGHTS, st['lights']):
        comp = light.get_component_by_class(unreal.DirectionalLightComponent)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_intensity(intensity)
        comp.set_cast_shadows(False)
    return True


def _place_cameras(follow=False):
    """follow=True (aerial sequences, every frame): the cameras keep the yaw of the sequence start and move with the pawn, which stays in the
    centre of the picture (the jump is 233 cm high, a fixed camera at 380 cm loses it); the height is in the rows ('h'), not in these pictures"""
    pawn = _pawn()
    centre, yaw = pawn.get_actor_location(), pawn.get_actor_rotation().yaw
    if follow:
        yaw = st.get('cam_yaw', yaw)
    else:
        st['cam_yaw'] = yaw
    if AHEAD:
        centre = unreal.Vector(centre.x + AHEAD * math.cos(math.radians(yaw)), centre.y + AHEAD * math.sin(math.radians(yaw)), centre.z)
    for (az, _intensity), light in zip(FILL_LIGHTS, st['lights']):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-25.0, yaw=yaw + az + 180.0), False)   # shines from azimuth az toward the pawn
    for (name, az), cam in zip(VIEWS, st['cams']):
        if az is None:
            continue
        a, e = math.radians(yaw + az), math.radians(ELEVATION)
        pos = unreal.Vector(centre.x + DISTANCE * math.cos(e) * math.cos(a), centre.y + DISTANCE * math.cos(e) * math.sin(a),
                            centre.z + DISTANCE * math.sin(e))
        cam.set_actor_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, centre), False, False)
    return (round(centre.x), round(centre.y), round(centre.z)), round(yaw)


def _shoot(seq, frame):
    manager = g._local_pawn(0)[0].get_editor_property('player_camera_manager')
    if seq.get('air') is not None or seq.get('follow'):
        _place_cameras(follow=True)
    for (name, az), cam, rt in zip(VIEWS, st['cams'], st['rts']):
        comp = cam.capture_component2d
        if az is None:
            cam.set_actor_location_and_rotation(manager.get_camera_location(), manager.get_camera_rotation(), False, False)
            comp.set_editor_property('fov_angle', manager.get_fov_angle())
        comp.capture_scene()
        folder = os.path.join(OUT, seq['name'], name)
        os.makedirs(folder, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(_world(), rt, folder, f'f{frame:03d}.png')


def _montage():
    """[montage name, position in 60 fps animation frames] of the first skeletal mesh that plays one, or None: what the picture really shows"""
    try:
        for comp in _pawn().get_components_by_class(unreal.SkeletalMeshComponent):
            anim = comp.get_anim_instance()
            montage = anim.get_current_active_montage() if anim else None
            if montage:
                return [montage.get_name(), round(anim.montage_get_position(montage) * 60.0, 2)]
    except Exception as e:
        return ['error', repr(e)[:80]]
    return None


def _fl_row(mesh, body):
    # NIGHT-FOOTLOCK: the blend-out foot lock's request and phases (UGSTN_CharacterAnimInstance; L*10+R, 0 free 1 locked 2 step 3 release)
    try:
        ai = mesh.get_anim_instance()
        body['fl'] = [int(ai.get_editor_property('foot_lock_debug_phase')), int(ai.get_editor_property('foot_lock_request')),
                      round(float(ai.get_editor_property('foot_lock_montage_weight')), 2)]
    except Exception as e:
        body['fl_err'] = repr(e)[:60]
    # STANCE-OFFSET: [debug state (0 none, 1 hidden, 2 delay, 3 returning/shown, 4 moving), seconds since the return started, rest alpha, montage scale]
    try:
        ai = mesh.get_anim_instance()
        body['st'] = [int(ai.get_editor_property('stance_debug_state')), round(float(ai.get_editor_property('stance_return_time')), 3),
                      round(float(ai.get_editor_property('stance_rest_alpha')), 2), round(float(ai.get_editor_property('stance_montage_scale')), 2)]
    except Exception as e:
        body['st_err'] = repr(e)[:60]


def _row(frame, tapped, extra=None):
    view = g._view(0, h._key(0)) or {}
    row = {'f': frame, 'tap': tapped, 'combat': view.get('combat'), 'tags': view.get('tags'), 'loc': view.get('location'), 'montage': _montage()}
    try:   # skid "two poses for the same input": actor and mesh yaw (world), logged every frame
        _pw = _pawn()
        _mesh = _pw.get_component_by_class(unreal.SkeletalMeshComponent)
        row['yaw'] = [round(_pw.get_actor_rotation().yaw, 2), round(_mesh.get_world_rotation().yaw, 2) if _mesh else None]
    except Exception as e:  # noqa: BLE001
        row['yaw_err'] = repr(e)[:80]
    row.update(extra or {})
    if st.get('dummy_key') is not None:   # PUSHBOX-P2: the dummy's state and place (knockdown tag, the push between the two)
        try:
            dv = g._view(0, st['dummy_key']) or {}
            row['dummy'] = {'tags': dv.get('tags'), 'loc': dv.get('location'), 'combat': dv.get('combat')}
            # P3: the step snapshot's action state, reaction flags and percent (tumble and knockdown are not in the GAS tags above)
            se = json.loads(g.comps(g.worlds()[0])[st['dummy_key']].read_snapshot_entry(-1) or '{}')
            row['dummy']['snap'] = {k: se.get(k) for k in ('actionState', 'reactionFlags', 'damagePercent', 'stateTags') if k in se}
        except Exception as e:
            row['dummy_err'] = repr(e)[:80]
    if os.environ.get('PIE_CAPTURE_HOST_SNAP') == '1':   # DROPKICK-BOUNCE-1: the host's own step snapshot (action state, state tags, reaction flags)
        try:
            se = json.loads(g.comps(g.worlds()[0])[h._key(0)].read_snapshot_entry(-1) or '{}')
            row['snap'] = {k: se.get(k) for k in ('actionState', 'reactionFlags', 'stateTags', 'grounded') if k in se}
        except Exception as e:
            row['snap_err'] = repr(e)[:80]
    try:   # body vs capsule (DROPKICK-CAPSULE): pelvis world z, capsule half height/radius, mesh z relative to the actor
        pawn = _pawn()
        cap = pawn.get_component_by_class(unreal.CapsuleComponent)
        mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
        row['body'] = {'pelvis_z': round(mesh.get_socket_location('pelvis').z, 1), 'actor_z': round(pawn.get_actor_location().z, 1),
                       'cap_hh': round(cap.get_unscaled_capsule_half_height(), 1), 'cap_r': round(cap.get_unscaled_capsule_radius(), 1),
                       'cap_bottom_z': round(pawn.get_actor_location().z - cap.get_unscaled_capsule_half_height(), 1),
                       'mesh_rel_z': round(mesh.get_editor_property('relative_location').z, 1)}
        # NIGHT-ROOT: root bone and feet, world, against the actor (root-capsule offset, foot creep)
        _al = pawn.get_actor_location()
        for _b in ('root', 'foot_l', 'foot_r', 'pelvis', 'head', 'hand_l', 'hand_r'):   # STANCE-OFFSET: head and hands (upper-body stance)
            _v = mesh.get_socket_location(_b)
            row['body'][_b] = [round(_v.x - _al.x, 1), round(_v.y - _al.y, 1), round(_v.z - _al.z, 1)]
        row['body']['actor'] = [round(_al.x, 1), round(_al.y, 1), round(_al.z, 1)]
        _fl_row(mesh, row['body'])
    except Exception as e:
        row['body_err'] = repr(e)[:80]
    try:   # LANDING-SYNC (2026-11-19): the ABP state of each machine and its time in it, to line the pose up with the sim's touchdown
        anim = _pawn().get_component_by_class(unreal.SkeletalMeshComponent).get_anim_instance()
        row['abp'] = {}
        for _m in ('State Controller', 'Locomotion'):
            _i = anim.get_state_machine_index(_m)
            if _i is not None and _i >= 0:
                row['abp'][_m] = [str(anim.get_current_state_name(_i)), round(anim.get_instance_current_state_elapsed_time(_i), 3)]
    except Exception as e:
        row['abp_err'] = repr(e)[:80]
    if st.get('minions_spawned'):   # PUSHBOX-P5: the host world's minion puppets [x, y, z] (hidden pooled ones left out)
        try:
            cls = getattr(unreal, 'GSTN_MinionPuppet', None)
            row['minions'] = [[round(l.x, 1), round(l.y, 1), round(l.z, 1)] for a in unreal.GameplayStatics.get_all_actors_of_class(g.worlds()[0], cls)
                              for c in [a.get_component_by_class(unreal.SkeletalMeshComponent)] if c is None or c.is_visible() for l in [a.get_actor_location()]] if cls else None
        except Exception as e:
            row['minions_err'] = repr(e)[:80]
    try:   # projectiles alive in the host world: [x, y, z, hidden, visual mesh set] (what a camera can see, 1 vs 2 shots)
        cls = getattr(unreal, 'GSTN_Projectile', None)
        if cls:
            row['proj'] = []
            for wi, w in enumerate(g.worlds()):   # every world: host (0) and the clients' ghosts
                for a in unreal.GameplayStatics.get_all_actors_of_class(w, cls):
                    l = a.get_actor_location()
                    row['proj'].append([wi, round(l.x), round(l.y), round(l.z)])
            # Shots the host fired in this sequence (each rise of the host's count), logged at its end: a capture of a shooting move that
            # shows no shot says so in capture.log instead of only in the rows
            host = sum(1 for p in row['proj'] if p[0] == 0)
            st['shots'] = st.get('shots', 0) + max(0, host - st.get('host_proj', 0))
            st['host_proj'] = host
    except Exception as e:
        row['proj_err'] = repr(e)[:80]
    st['rows'].write(json.dumps(row) + '\n')


def _run_asserts():
    """Project rule 10 (2026-11-20): a review starts only on ASSERT: OK. Expectations come from each sequence's 'expect' entry
    ({'clip', 'window': [first, last] capture frames, 'min_weight'?, 'coverage'?} or a list of them) and from the JSON file named by
    PIE_CAPTURE_ASSERT ({sequence: [expect, ...]}, same format as Tools/AnimPipeline/capture_assert.py --spec). Writes <out>/assert.txt
    (first line = the gate) and logs it. No expectation at all = ASSERT: FAIL (nothing proves the video shows the clip)."""
    sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'AnimPipeline'))
    import capture_assert as ca
    spec = {}
    for seq in SEQUENCES:
        if seq.get('expect'):
            spec[seq['name']] = seq['expect'] if isinstance(seq['expect'], list) else [seq['expect']]
    path = os.environ.get('PIE_CAPTURE_ASSERT')
    if path:
        for name, exp in json.load(open(path, encoding='utf-8')).items():
            if not name.startswith('_'):
                spec.setdefault(name, []).extend(exp if isinstance(exp, list) else [exp])
    ran = {s['name'] for s in SEQUENCES}
    spec = {k: v for k, v in spec.items() if k in ran}
    ok, results = ca.check_spec(spec, OUT)
    lines = ca.summary_lines(ok, results)
    with open(os.path.join(OUT, 'assert.txt'), 'w', encoding='utf-8') as f:
        f.write(lines[0] + '\n' + '\n'.join(ca.line(r, s) for s, r in results) + '\n')
    for ln in lines[:5]:
        _log(ln)


def _finish(status):
    _log(f'finish: {status}')
    st['phase'] = 'ending'
    st['status'] = status
    try:
        _console('t.OverrideFPS 0')
    except Exception:
        pass
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()


def _tick(_dt):
    try:
        now, phase = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase != 'ending' and now - st['start'] > TIMEOUT:
            return _finish(f'timeout in phase {phase}')
        if phase == 'boot' and now - st['t'] > 8:
            # PIE_CAPTURE_CLIENTS (default 2 = 3 players): 0 for single-character captures (locomotion): 1 PIE world, ~3x faster (speed-up.md 4)
            _log(f'setup_editor: {g.setup_editor(True, "listen", int(os.environ.get("PIE_CAPTURE_CLIENTS", "2")))}')
            les.editor_request_begin_play()
            st.update(phase='wait_ready', t=now)
        elif phase == 'wait_ready':
            if les.is_in_play_in_editor() and _ready():
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready after 180 s')
        elif phase == 'settle' and now - st['t'] > 12:     # the hub's watcher runs play_setup 4 s after Play (dummies, kits, hit rules)
            _log(f'kit {KIT}: {h.kit(0, KIT)}')
            st.update(phase='kit_wait', t=now)
        elif phase == 'kit_wait' and now - st['t'] > 3:
            # F4 (2026-11-17): the two clients' players can join after the host's input is ready; parking ran with [] and a hub pawn stood
            # next to ours in the R4/playtest_r3 captures. Wait up to 15 s for all three players before staging
            if len(h._players()) < 3 and now - st['t'] < 15:
                return
            view = g._view(0, h._key(0)) or {}
            _log(f'loadout: {json.dumps(view.get("loadout"))[:300]}')
            _log(f'review stage: {_clear_stage()}')
            if os.environ.get('PIE_CAPTURE_PARK'):   # 'Z7:1' = test_hub.park(host, zone, slot): slope captures
                _z, _, _slot = os.environ['PIE_CAPTURE_PARK'].partition(':')
                _log(f'park host: {h.park(0, _z, int(_slot) if _slot else None)}')
            st.update(phase='cameras', t=now)
        elif phase == 'cameras':
            if _make_cameras() and _make_lights():
                _log(f'{len(st["cams"])} cameras, {len(st["lights"])} fill lights')
                st.update(phase='seq_begin', t=now)
            elif now - st['t'] > 20:
                _finish('could not summon the scene captures')
        elif phase == 'seq_begin' and now - st['t'] > 2:
            if st['seq'] >= len(SEQUENCES):
                return _finish('ok')
            seq = SEQUENCES[st['seq']]
            if seq['dummy'] and not st.get('dummy_placed'):
                keys = h.dummies()
                _log(f'dummy {keys[:1]} ahead: {h._place_ahead(0, keys[0], seq["dummy"]) if keys else "NO DUMMY"}')
                if keys and seq.get('dummy_percent') is not None:   # test harness only (server write of the health set's DamagePercent)
                    ok = g.comps(g.worlds()[0])[keys[0]].server_set_damage_percent(seq['dummy_percent'])
                    _log(f'dummy {keys[0]} percent {seq["dummy_percent"]}: {ok}')
                st.update(dummy_placed=True, dummy_key=keys[0] if keys else None, t=now)         # one more wait for the teleport to settle
                return None
            if seq.get('minions') and not st.get('minions_spawned'):
                count, mode = seq['minions']
                _console('gstn.Minion.Bench.Clear')
                _console(f'gstn.Minion.Bench.Spawn {count} {mode}')
                _log(f'{seq["name"]}: spawned {count} {mode} minions')
                st.update(minions_spawned=True, t=now)               # one more wait: the spawns activate and land
                return None
            if seq.get('air') is not None:
                z = _pawn().get_actor_location().z
                if (st.get('rest_z') is None or abs(z - st['rest_z']) > 0.05) and now - st['t'] < 10:
                    st['rest_z'] = z
                    return None                              # still falling or landing from the last sequence
                # Per sequence: the standing height is not the same before and after the first landing (100 then 88 cm on the first run)
                st['ground_z'] = z
                _log(f'{seq["name"]}: standing height z {z:.1f}')
            _log(f'sequence {seq["name"]}: pawn at {_place_cameras()}')
            os.makedirs(os.path.join(OUT, seq['name']), exist_ok=True)
            st['rows'] = open(os.path.join(OUT, seq['name'], 'frames.jsonl'), 'w')
            _console('t.OverrideFPS 60')
            if os.environ.get('PIE_CAPTURE_PUSHLOG') == '1':
                _console('gstn.Pushbox.Log 1')
            st.update(shots=0, host_proj=0)
            st.update(phase='seq_run', frame=0, mask=g.button_mask(BUTTON), crouch=g.button_mask('Crouch'), jumped=None, fired=None, prev_z=None)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            if seq['shoot']:
                _shoot(seq, frame)
            # The scene captures do not show the debug draw (hitboxes, body): PIE_CAPTURE_HIRES='86,91' takes a real viewport shot on those
            # frames (Saved/Screenshots/WindowsEditor/HighresScreenshot*.png, in frame order)
            if str(frame) in os.environ.get('PIE_CAPTURE_HIRES', '').split(','):
                _console('HighResShot 1')
                _log(f'{seq["name"]}: HighResShot on v{frame}')
            tapped = (frame - PRE) in seq['taps']
            extra, hold_crouch = None, False
            air = seq.get('air')
            if air is not None:
                z = _pawn().get_actor_location().z
                height = z - st.get('ground_z', z)
                extra = {'h': round(height, 1)}
                rising = st['prev_z'] is not None and z > st['prev_z']
                falling = st['prev_z'] is not None and z < st['prev_z']
                if 'rise_h' in air:
                    reached = rising and height >= air['rise_h']
                    if reached and air.get('press_delay'):   # capture tuning: press this many frames after the height was reached
                        st.setdefault('reached_at', frame)
                        reached = frame - st['reached_at'] >= air['press_delay']
                elif 'trigger_h' in air:
                    reached = falling and height <= air['trigger_h']
                else:
                    reached = None                           # a plain jump: nothing is pressed
                if frame == PRE:
                    st['jumped'] = frame
                    g.press(0, 'jump')
                elif reached is not None and st['jumped'] is not None and st['fired'] is None and frame > PRE + 3 and (
                        reached or frame - PRE >= AIR_GIVE_UP):
                    st['fired'] = frame + 1                  # crouch now, Attack on the next frame
                    hold_crouch = True
                    _log(f'{seq["name"]}: crouch on v{frame} (height {height:.0f} cm, {"rising" if rising else "falling"}), attack on v{frame + 1}')
                tapped = st['fired'] is not None and frame == st['fired']
                st['prev_z'] = z
            _row(frame, tapped, extra)
            if hold_crouch:
                if air.get('stick'):
                    g.hold(0, 'move', (0, 1, 0))            # stick forward (relative to the camera, as in scenario_combat_main_inputs)
                else:
                    g.combat_hold(0, st['crouch'])
            if tapped:
                g.combat_tap(0, st['mask'])                 # read by the next simulation tick
            if air is not None and st['fired'] is not None and frame == st['fired'] + AIR_CROUCH_RELEASE:
                g.combat_clear(0)
                if air.get('stick'):
                    g.release(0, 'move')
            if seq.get('marker') and frame == PRE - 5:
                _console('gstn.DepthMarker.Enable 1')
            if seq.get('proj_volume') and frame == PRE:
                try:
                    w = g.worlds()[0]
                    loc = _pawn().get_actor_location()
                    cls = unreal.GSTN_DepthProjectionVolume
                    tf = unreal.Transform(loc, unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
                    vol = None
                    for fn in ('begin_spawning_actor_from_class', 'begin_deferred_actor_spawn_from_class'):
                        if hasattr(unreal.GameplayStatics, fn):
                            vol = getattr(unreal.GameplayStatics, fn)(w, cls, tf)
                            vol = unreal.GameplayStatics.finish_spawning_actor(vol, tf)
                            break
                    if vol is None and hasattr(unreal.EditorLevelLibrary, 'spawn_actor_from_class'):
                        _log('spawn candidates: ' + ','.join(n for n in dir(unreal.GameplayStatics) if 'pawn' in n))
                        raise RuntimeError('no PIE-world spawn function')
                    pr = unreal.GSTN_DepthProjection()
                    pr.set_editor_property('mode', unreal.GSTN_DepthProjectionMode.OWNER_LOCAL)
                    pr.set_editor_property('direction', unreal.Vector(0, 1, 0))
                    box = vol.get_editor_property('box')
                    box.set_editor_property('projection', pr)
                    box.set_editor_property('priority', 10)
                    box.set_box_extent(unreal.Vector(1500, 1500, 600), True)
                    _log(f'{seq["name"]}: projection volume spawned at {loc}')
                except Exception as e:  # noqa: BLE001
                    _log(f'{seq["name"]}: projection volume FAILED: {e!r}')
            for at, value in seq.get('sticks', []):                 # movement QA clips
                if frame == PRE + at:
                    if value is None:
                        g.release(0, 'move')
                    else:
                        g.hold(0, 'move', value)
                    _log(f'{seq["name"]}: stick {value} on v{frame}')
            if seq.get('jump') is not None:
                at, held = seq['jump']
                if frame == PRE + at:
                    jump_mask = g.button_mask('Jump')
                    g.combat_tap(0, jump_mask)
                    if held > 0:
                        g.combat_hold(0, jump_mask)
                    _log(f'{seq["name"]}: jump on v{frame}, held {held} frames')
                elif held > 0 and frame == PRE + at + held:
                    g.combat_clear(0)
            if seq.get('boost') is not None and frame == PRE + seq['boost']:   # dash-ganon: the L3 run boost through its combat button
                # the boost is an input-data key read by the locomotion profile (not a combat button: a combat tap did nothing, dash r3 cap1)
                g.IA.setdefault('runboost', '/Game/GASNPPTest/Input/IA_GASNPP_RunBoost')
                g.press(0, 'runboost')
                _log(f'{seq["name"]}: boost (IA_GASNPP_RunBoost press) on v{frame}')
            if seq.get('crouch_tap') is not None and frame == PRE + seq['crouch_tap']:
                g.combat_tap(0, st['crouch'])
                _log(f'{seq["name"]}: crouch tap on v{frame}')
            if seq.get('crouch_hold') is not None:
                at, held = seq['crouch_hold']
                if frame == PRE + at:
                    g.combat_hold(0, st['crouch'])
                    _log(f'{seq["name"]}: crouch held from v{frame}')
                elif frame == PRE + at + held:
                    g.combat_clear(0)
                    _log(f'{seq["name"]}: crouch released on v{frame}')
            walk = seq.get('walk')
            # From the attack press; a plain jump (no press) from the jump; a ground sequence with no air entry from PRE (pushbox P4)
            plain_jump = air is not None and 'rise_h' not in air and 'trigger_h' not in air
            walk_base = st['fired'] if st['fired'] is not None else (st['jumped'] if plain_jump else (PRE if air is None else None))
            if walk and walk_base is not None:
                if frame == walk_base + walk[0]:
                    g.hold(0, 'move', seq.get('walk_dir', (0, 1, 0)))
                    _log(f'{seq["name"]}: walk forward from v{frame}')
                elif frame == walk_base + walk[0] + walk[1]:
                    g.release(0, 'move')
            st['frame'] = frame + 1
            if st['frame'] >= PRE + seq['frames']:
                st['rows'].close()
                g.combat_clear(0)
                if seq.get('sticks'):
                    g.release(0, 'move')
                _console('t.OverrideFPS 0')
                _log(f'sequence {seq["name"]} done: {st["frame"]} frames, pawn z {_pawn().get_actor_location().z:.1f}, host shots {st.get("shots", 0)}')
                if seq.get('dummy_percent') is not None and st.get('dummy_key'):   # NIGHT-KD-1: why the landing did (not) down the dummy
                    try:
                        dummy = g.comps(g.worlds()[0])[st['dummy_key']].get_owner()
                        sim = dummy.get_component_by_class(unreal.GSTN_CombatSimComponent)
                        _log(f'dummy impact debug: {sim.get_impact_debug() if sim else "no sim"}')
                        _log(f'dummy defence: {unreal.GSTN_DefenceContentLibrary.get_defence_state_string(dummy)}')
                    except Exception as e:  # noqa: BLE001
                        _log(f'dummy diag failed: {e}')
                if st.get('minions_spawned'):
                    _console('gstn.Minion.Bench.Clear')
                st.update(phase='seq_begin', seq=st['seq'] + 1, t=now, dummy_placed=False, minions_spawned=False)
        elif phase == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:
                _log(f'setup_editor(False) failed: {e!r}')
            try:
                _run_asserts()
            except Exception as e:  # noqa: BLE001
                _log(f'ASSERT: FAIL assert step crashed: {e!r}')
                with open(os.path.join(OUT, 'assert.txt'), 'w') as f:
                    f.write(f'ASSERT: FAIL assert step crashed: {e!r}\n')
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write(st.get('status', '?') + '\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:
        _log('ERROR ' + traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error (see capture.log)')
        else:
            unreal.unregister_slate_post_tick_callback(st['handle'])
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write('error while ending\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    return None


# NIGHT-FOOTLOCK: console variables set before Play (';' separated, as in pie_capture_react_ingame.py), e.g. a.GSTN.BlendOutFootLock 0
for _cv in [c.strip() for c in os.environ.get('PIE_CAPTURE_CVARS', '').split(';') if c.strip()]:
    unreal.SystemLibrary.execute_console_command(None, _cv)
    _log(f'cvar: {_cv}')
_log(f'driver loaded: kit {KIT}, button {BUTTON}, out {OUT}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
