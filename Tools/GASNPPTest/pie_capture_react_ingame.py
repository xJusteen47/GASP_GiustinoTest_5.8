# REACT-HOOK-1: a Play capture of the victim reactions IN GAME. Unlike pie_capture_react.py (clips played in place on the anim instance), nothing
# is played by this script: client 1's pawn really hits the host pawn with the test move, and what the host pawn shows comes from the game
# (UGSTN_ReactionAnimComponent reading Combat.State.*, the get-up abilities and their travel). Same stage as pie_capture_react.py: Play in L_Test_Hub,
# listen host + 2 clients, the third pawn parked in Z1, fixed fill lights, t.OverrideFPS 60 so one image = one 60 fps frame.
#
# Launch (editor with a real renderer; the script path must have no space, so boot through a one-line exec in a path without spaces):
#   UnrealEditor.exe <uproject> /Game/GSTN/Maps/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/pie_capture_react_ingame.py").read())
# Environment (optional): PIE_CAPTURE_OUT (default Saved/AnimReact/ingame_<date>_<time>), PIE_CAPTURE_SEQUENCES (subset of hitstand,
# knockdown_getup, roll, rollback, roll_late, back_hitstand, back_knockdown).
# Content in memory only (nothing is saved): both pawns get KIT_Test_DefenceLanding (the landing bounce, the downed state with no automatic get-up,
# the get-up set with the roll back) through defence_scenarios.defence_setup(landing=True); hitstand uses the standard knockback thresholds at 0 %,
# the other sequences the thresholds at 0 (every hit launches into tumble).
# Sequences (frames at 60 fps, PRE idle frames first, then client 1 taps Attack):
#   hitstand         the hit and the hit stand, until the pawn is back in the game pose
#   knockdown_getup  launch -> tumble -> landing (bounce, ground hit chained into the lie), lying LIE frames, then the jump press: the neutral get-up
#   roll / rollback  the same landing, then (ROLL_LIE frames into the knockdown) the host holds the stick along / against its facing: the get-up roll toward the feet / the head
# Views: az180 (behind the pawn) and az90 (its right side), fixed azimuth from the pawn's yaw at the sequence start; the cameras follow the pawn
# only when it leaves a DEADZONE cm circle around their aim point (the launch travels several metres), so short travel (the rolls) stays visible.
# Output: <out>/<sequence>/<view>/f<frame>.png, <out>/<sequence>/frames.jsonl (f, seg = the host's combat state, segf = frames in it, montage,
# loc, tap), <out>/capture.log, <out>/done.txt. The mp4 and the sheets come from pie_capture_grid.py.
import json
import math
import os
import sys
import time
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g          # noqa: E402
import test_hub as h             # noqa: E402
import defence_scenarios as ds   # noqa: E402

OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimReact',
                                                        time.strftime('ingame_%Y%m%d_%H%M%S'))
SIZE = 480
# react-hook-2: the victim faces the attacker (it was hit from behind in react-hook-1). az45 = in front of the victim, a quarter off the attacker's
# line (the attacker is hidden in it), az90 = its right side with both pawns in profile
VIEWS = [('az45', 45.0), ('az90', 90.0)]
if os.environ.get('PIE_CAPTURE_VIEWS'):
    VIEWS = [(v.split(':')[0], float(v.split(':')[1])) for v in os.environ['PIE_CAPTURE_VIEWS'].split(',')]
HIDE_ATTACKER = {'az45'}
ELEVATION, DISTANCE, FOV = 10.0, 320.0, 45.0
DEADZONE = 40.0   # the pawn never leaves the middle of the frame (react-hook-1: cut at the edge)
FILL_LIGHTS = [(35.0, 5.0), (325.0, 5.0), (95.0, 3.0)]
PRE = 10
LIE = 50          # frames lying after the knockdown starts before the get-up input (the ground hit and the lie must read)
AFTER = 70        # frames shot after the get-up ends (the held pose blending out, the game pose settling)
MAX_FRAMES = 420
STATES = ['Hitstun', 'Tumble', 'Knockdown', 'Splat']
SEQUENCES = [
    {'name': 'hitstand', 'launch': False, 'getup': None},
    {'name': 'knockdown_getup', 'launch': True, 'getup': 'jump'},
    {'name': 'roll', 'launch': True, 'getup': 'facing'},
    {'name': 'rollback', 'launch': True, 'getup': 'against'},
    {'name': 'roll_late', 'launch': True, 'getup': 'facing', 'lie': 50},   # the stick only after lying 50 frames: a roll on any frame while down
    # react-hook-4: hit from BEHIND (the victim's back to the attacker): bTurnBackAlongKnockback turns it back first along the knockback
    {'name': 'back_hitstand', 'launch': False, 'getup': None, 'behind': True},
    {'name': 'back_knockdown', 'launch': True, 'getup': 'jump', 'behind': True},
    # hit-react-int: the victim stands SIDEWAYS to the attacker (turned 90 deg off the line to it): directional additive check (right = side +1)
    {'name': 'side_hitstand', 'launch': False, 'getup': None, 'side': 1},
    {'name': 'side_hitstand_l', 'launch': False, 'getup': None, 'side': -1},
]
if os.environ.get('PIE_CAPTURE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES if s['name'] in os.environ['PIE_CAPTURE_SEQUENCES'].split(',')]
# getup-momentum-1 (2026-11-21): get-up variants in ONE Play session. PIE_CAPTURE_GETUP_RUNS = a JSON file with a list of
#   {"name": "roll_run_v220", "getup": "jump"|"facing"|"against", "follow": "idle"|"run"|"jab", "cvars": {"gstn.Getup.RollExitSpeed": 220}}
#   getup-momentum-2: "angle": degrees the roll's stick is turned off the get-up axis (+ = to the right of it, seen from above: UE yaw), held 10 frames
#   (the directional roll turns while it is held); 'run' then pushes along the NEW facing at FAF
#   "hold": true keeps the roll's stick held from the roll input to the end (straight run-out, no re-press at FAF)
# follow: idle = no input after the get-up starts (the tail plays out), run = stick toward the get-up's travel (the facing for the stand-up)
# held from the get-up start (ignored until FAF, then it cuts the tail), jab = Attack tapped on the FAF frame. cvars are set when the
# sequence starts and put back to -1 when it ends. Replaces SEQUENCES.
GETUP_MONTAGE = {'jump': 'AM_React_GetUp', 'facing': 'AM_React_Roll', 'against': 'AM_React_RollBack'}
GETUP_ABILITY = {'jump': 'GA_GetupNeutral_Test', 'facing': 'GA_GetupRoll_Test', 'against': 'GA_GetupRollBack_Test'}
if os.environ.get('PIE_CAPTURE_GETUP_RUNS'):
    with open(os.environ['PIE_CAPTURE_GETUP_RUNS']) as _fh:
        SEQUENCES = [dict(launch=True, **r) for r in json.load(_fh)]
# Views (getup-momentum-1: behind AND side): PIE_CAPTURE_VIEWS = 'az180:180,az90:90'; shoot only from the get-up input on with
# PIE_CAPTURE_SHOOT_FROM_INPUT=1 (the launch and the flight are the same in every variant)
SHOOT_FROM_INPUT = os.environ.get('PIE_CAPTURE_SHOOT_FROM_INPUT') == '1'
ATTACKER = 1
KIT = 'KIT_Test_DefenceLanding'
ROLL_LIE = 4      # roll sequences: the stick is held from this many frames after the knockdown starts; the roll must wait for the landing
FACE_SECONDS = float(os.environ.get('PIE_CAPTURE_FACE_SECONDS') or 0.3)  # the victim walks toward the attacker this long to face it, from PLACE cm away
PLACE = 245.0
PLACE_SIDE = float(os.environ.get('PIE_CAPTURE_PLACE_SIDE') or 120.0)   # hit-react-int: side sequences start this close (the turn walks it away)
PLACE_BEHIND = float(os.environ.get('PIE_CAPTURE_PLACE_BEHIND') or 165.0)   # behind: the walk away adds about the 40 cm the walk toward takes off
BEHIND_STOP = float(os.environ.get('PIE_CAPTURE_BEHIND_STOP') or 160.0)  # behind: release the stick once turned away and this far (cm); it slides a bit on
TIMEOUT = 1500.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'cams': [], 'rts': [], 'lights': [], 'handle': None,
      'rows': None, 'aim': None, 'yaw': 0.0}


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
    if len(ws) < 3 or _pawn() is None:
        return False
    comp = g.local_comp(ws[0])
    return bool(comp and json.loads(comp.get_state_snapshot()).get('inputReady'))


def _mesh():
    pawn = _pawn()
    try:
        mesh = pawn.get_editor_property('mesh')
        if mesh and mesh.get_anim_instance():
            return mesh
    except Exception:
        pass
    for comp in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_anim_instance():
            return comp
    return None


def _make_cameras():
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
    return True


def _make_lights():
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


def _clear_stage():
    """Parks every player but the host (victim) and client 1 (attacker); hides the static meshes on the host pawn (kit placeholder)"""
    parked = [h.park(i, 'Z1') for i in h._players() if i not in (0, ATTACKER)]
    _console('gstn.Combat.KitPlaceholderLook 0')
    for comp in _pawn().get_components_by_class(unreal.StaticMeshComponent):
        comp.set_visibility(False, True)
    for a in _pawn().get_attached_actors():
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            comp.set_visibility(False, True)
    return f'parked {parked}'


def _hide_from(cam, view):
    """Every pawn but the victim (and the attacker, except in the views of HIDE_ATTACKER) is hidden from this capture"""
    victim = _pawn()
    attacker = g._local_pawn(ATTACKER)[1] if len(g.worlds()) > ATTACKER else None
    hidden = []
    for p in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Pawn):
        if p == victim:
            continue
        if attacker is not None and _same_player(p, attacker) and view not in HIDE_ATTACKER:
            continue
        hidden.append(p)
    cam.capture_component2d.set_editor_property('hidden_actors', hidden)


def _same_player(server_pawn, client_pawn):
    """The server world's copy of a client's pawn: the attacker's location read in the server snapshot"""
    try:
        alo = g.snap(_world())[st['akey']]['location']
        loc = server_pawn.get_actor_location()
        return math.dist((loc.x, loc.y, loc.z), alo) < 5.0
    except Exception:
        return False


def _aim_cameras():
    """Moves the aim point toward the pawn only past DEADZONE, then places the cameras around it at the sequence's fixed azimuths"""
    loc = _pawn().get_actor_location()
    if st['aim'] is None:
        st['aim'] = [loc.x, loc.y, loc.z]
    ax, ay, az_ = st['aim']
    dx, dy = loc.x - ax, loc.y - ay
    d = math.hypot(dx, dy)
    if d > DEADZONE:
        k = (d - DEADZONE) / d
        ax, ay = ax + dx * k, ay + dy * k
    az_ = az_ + (loc.z - az_) * 0.3 if abs(loc.z - az_) > 30.0 else az_
    st['aim'] = [ax, ay, az_]
    centre = unreal.Vector(ax, ay, az_)
    for (name, azim), cam in zip(VIEWS, st['cams']):
        a, e = math.radians(st['yaw'] + azim), math.radians(ELEVATION)
        pos = unreal.Vector(centre.x + DISTANCE * math.cos(e) * math.cos(a), centre.y + DISTANCE * math.cos(e) * math.sin(a),
                            centre.z + DISTANCE * math.sin(e))
        cam.set_actor_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, centre), False, False)


def _place_lights():
    for (az, _intensity), light in zip(FILL_LIGHTS, st['lights']):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-25.0, yaw=st['yaw'] + az + 180.0), False)


def _shoot(seq, frame):
    _aim_cameras()
    for (name, _az), cam, rt in zip(VIEWS, st['cams'], st['rts']):
        _hide_from(cam, name)
        cam.capture_component2d.capture_scene()
        folder = os.path.join(OUT, seq['name'], name)
        os.makedirs(folder, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(_world(), rt, folder, f'f{frame:03d}.png')


def _states():
    return [s for s in STATES if ds.has_tag(0, st['hkey'], 'Combat.State.' + s)]


def _montage():
    try:
        anim = _mesh().get_anim_instance()
        m = anim.get_current_active_montage()
        return [m.get_name(), round(anim.montage_get_position(m) * 60.0, 2)] if m else None
    except Exception as e:
        return ['error', repr(e)[:80]]


# ---- the sequence: frame-driven, one tick = one 60 fps frame -------------------------------------------------------------------------------
def _seq_prep(seq):
    if _states():   # still downed from the sequence before: get up first (jump), the caller waits
        g.release(0, 'move')
        g.combat_tap(0, g.button_mask('Jump'))
        return False
    g.hit_rules_setup()
    if seq['launch']:
        g.set_tuning(push_back_threshold=0.0, launch_threshold=0.0, tumble_threshold=0.0)
    g.set_damage(st['hkey'], 0)
    g.release(0, 'move')
    # In FRONT of the attacker and facing it (react-hook-1 placed it with its back to the attacker): a short walk toward the attacker turns it,
    # then the pawn stops about 205 cm away (the test move's reach)
    # Behind: placed closer and walks AWAY from the attacker, so it ends about as far, with its back to it
    g._place_near(ATTACKER, st['hkey'], (PLACE_BEHIND if seq.get('behind') else (PLACE_SIDE if seq.get('side') else PLACE), 0.0))
    return True


def _turn_to_attacker(behind=False, side=0):
    """After the teleport has landed (react-hook-2: steering from the pre-teleport location walked the victim the wrong way)"""
    alo = g.snap(_world())[st['akey']]['location']
    if behind:
        loc = _pawn().get_actor_location()
        alo = (2.0 * loc.x - alo[0], 2.0 * loc.y - alo[1])
    if side:   # steer toward a point 90 deg off the line to the attacker
        loc = _pawn().get_actor_location()
        dx, dy = alo[0] - loc.x, alo[1] - loc.y
        n = math.hypot(dx, dy) or 1.0
        alo = (loc.x - side * dy / n * 300.0, loc.y + side * dx / n * 300.0)
        st['side_head'] = math.degrees(math.atan2(alo[1] - loc.y, alo[0] - loc.x))
    g.hold(0, 'move', g._steer_value(0, (alo[0], alo[1])))


def _stick(direction, angle=0.0):
    """Holds the stick along (facing) or against the facing, turned by angle degrees (getup-momentum-2: the directional roll)"""
    loc = _pawn().get_actor_location()
    yaw = math.radians(_pawn().get_actor_rotation().yaw + (0.0 if direction == 'facing' else 180.0) + angle)
    target = (loc.x + 300.0 * math.cos(yaw), loc.y + 300.0 * math.sin(yaw))
    g.hold(0, 'move', g._steer_value(0, target))
    return round(math.degrees(yaw))


def _step(seq, frame, states):
    """Inputs of this frame; returns (tap text or None, done)"""
    s = st['run']
    down = 'Knockdown' in states
    if frame == PRE:
        g.combat_tap(ATTACKER, g.button_mask('Attack'))
        return 'attack c1', False
    if frame <= PRE + 5:
        return None, False
    if not seq['launch']:
        if not states and s.get('hit') and s.setdefault('free_at', frame) + AFTER <= frame:
            return None, True
        if states:
            s['hit'] = True
            s.pop('free_at', None)
        return None, frame >= MAX_FRAMES
    if down and 'down_at' not in s:
        s['down_at'] = frame
    tap = None
    if 'down_at' in s and 'input_at' not in s and frame >= s['down_at'] + seq.get('lie', LIE if seq['getup'] == 'jump' else ROLL_LIE):
        s['input_at'] = frame
        if seq['getup'] == 'jump':
            g.combat_tap(0, g.button_mask('Jump'))
            tap = 'jump'
        else:
            tap = f'stick {seq["getup"]} yaw {_stick(seq["getup"], seq.get("angle", 0.0))}'
    if 'input_at' in s and not down and 'up_at' not in s:
        s['up_at'] = frame
    if seq.get('follow'):
        # Since 2026-11-17 a kit may want a dodge press with the stick for the roll (FGSTN_TumbleLanding::WantsDownedRoll, DodgeAndStick):
        # tap it every 6 frames while still down and the stick is held
        if seq['getup'] != 'jump' and down and 'input_at' in s and (frame - s['input_at']) % 6 == 0:
            g.combat_tap(0, g.button_mask('Dodge'))
            tap = (tap + '; ' if tap else '') + 'dodge'
        follow_tap, follow_done = _follow(seq, frame)
        return follow_tap or tap, follow_done or frame >= MAX_FRAMES
    if 'up_at' in s and seq['getup'] != 'jump' and 'released' not in s and not states and frame >= s['up_at'] + 26:
        g.release(0, 'move')   # the stick held past the roll would walk the pawn: let go once the roll is over (24 frames)
        s['released'] = frame
        tap = 'stick released'
    done = ('up_at' in s and frame >= s['up_at'] + 26 + AFTER) or frame >= MAX_FRAMES
    return tap, done


def _getup_faf_video(getup):
    """The get-up ability's FAF in 60 fps video frames from its start (sim frames at 62.5 Hz), from the class default object"""
    name = GETUP_ABILITY[getup]
    cls = unreal.load_class(None, f'/GSTNDefence/Test/{name}.{name}_C')   # load_blueprint_class gave None for the plugin path in PIE
    tl = unreal.get_default_object(cls).get_editor_property('timeline')
    faf, total = int(tl.get_editor_property('first_actionable_frame')), int(tl.get_editor_property('total_frames'))
    return math.ceil(faf * 60.0 / 62.5), math.ceil(total * 60.0 / 62.5), faf, total


def _follow(seq, frame):
    """getup-momentum-1 inputs after the get-up input: returns (tap text or None, done). The get-up start is the first frame its montage plays"""
    s = st['run']
    tap = None
    m = _montage()
    if 'input_at' in s and 'gu_at' not in s and m and m[0] == GETUP_MONTAGE[seq['getup']]:
        s['gu_at'] = frame
        s['faf_v'], s['total_v'], s['faf'], s['total'] = _getup_faf_video(seq['getup'])
        tap = f'getup started (FAF sim {s["faf"]}/{s["total"]}, video +{s["faf_v"]})'
        # The roll's stick is let go once the roll has started. A stick HELD through the get-up does not cut the tail (new3: the tail cancel
        # wants a stick move), so 'run' pushes it again on the FAF frame, toward the get-up's travel. getup-momentum-2: with an 'angle' (the
        # directional roll) the stick stays held until the turn is over (gstn.Getup.WarpFrames, about 6 frames), so it is let go later
        # getup-momentum-2 "hold": the stick stays held through the get-up (a roll's tail is 3 frames, a press on the FAF frame comes too late)
        if not seq.get('angle') and not seq.get('hold'):
            g.release(0, 'move')
    if 'gu_at' not in s:
        return tap, False
    at = frame - s['gu_at']
    if seq.get('angle') and at == 10 and seq['follow'] != 'run':
        g.release(0, 'move')
        tap = 'stick released after the turn'
    if seq['follow'] == 'run' and at == s['faf_v']:
        tap = f'stick {"against" if seq["getup"] == "against" else "facing"} yaw {_stick("against" if seq["getup"] == "against" else "facing")} at getup+{at}'
    if seq['follow'] == 'jab' and at in (s['faf_v'], s['faf_v'] + 2):
        g.combat_tap(0, g.button_mask('Attack'))
        tap = f'attack at getup+{at}'
    if seq['follow'] == 'run' and at == s['faf_v'] + 45 and 'released' not in s:
        g.release(0, 'move')
        s['released'] = frame
        tap = 'stick released'
    return tap, at >= s['total_v'] + AFTER


def _assert_lines():
    """ASSERT (rule 10): in every get-up sequence the get-up montage plays with weight >= 0.8 from its start to its FAF"""
    bad = []
    for seq in SEQUENCES:
        if not seq.get('follow'):
            continue
        path = os.path.join(OUT, seq['name'], 'frames.jsonl')
        rows = [json.loads(line) for line in open(path)] if os.path.exists(path) else []
        start = next((r['f'] for r in rows if r.get('montage') and r['montage'][0] == GETUP_MONTAGE[seq['getup']]), None)
        faf_v = next((int(r['tap'].split('video +')[1].split(')')[0]) for r in rows if 'video +' in r.get('tap', '')), None)
        if start is None or faf_v is None:
            bad.append(f'{seq["name"]}: no get-up')
            continue
        window = [r for r in rows if start <= r['f'] < start + faf_v]
        # weight = the anim instance's largest montage weight (foot lock debug); rows without it count on the montage name alone
        good = [r for r in window if r.get('montage') and r['montage'][0] == GETUP_MONTAGE[seq['getup']] and (r['fl'][2] if 'fl' in r else 1.0) >= 0.8]
        if len(good) < 0.9 * max(len(window), 1):
            bad.append(f'{seq["name"]}: {len(good)}/{len(window)} frames')
    return 'ASSERT: OK' if not bad else 'ASSERT: FAIL ' + '; '.join(bad)


def _row(frame, states, tap):
    s = st['run']
    key = '+'.join(states) if states else ('getup' if 'input_at' in s and 'up_at' in s and frame < s['up_at'] + 26 else 'game')
    if key != s.get('seg'):
        s['seg'], s['seg0'] = key, frame
    loc = _pawn().get_actor_location()
    row = {'f': frame, 'seg': key, 'segf': frame - s['seg0'], 'montage': _montage(), 'loc': [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
           'yaw': round(_pawn().get_actor_rotation().yaw, 1)}
    # NIGHT-ABP: world positions of the feet, root and pelvis of the drawn mesh (foot creep / sag / crouch measured against the capsule)
    try:
        mesh = _pawn().get_component_by_class(unreal.SkeletalMeshComponent)
        for b in ('foot_l', 'foot_r', 'root', 'pelvis'):
            v = mesh.get_socket_location(b)
            row[b] = [round(v.x, 1), round(v.y, 1), round(v.z, 1)]
        try:   # NIGHT-FOOTLOCK: blend-out foot lock phases (L*10+R: 0 free 1 locked 2 step 3 release), request, largest montage weight
            ai = mesh.get_anim_instance()
            row['fl'] = [int(ai.get_editor_property('foot_lock_debug_phase')), int(ai.get_editor_property('foot_lock_request')),
                         round(float(ai.get_editor_property('foot_lock_montage_weight')), 2)]
        except Exception as e:  # noqa: BLE001
            row['fl_err'] = repr(e)[:60]
        rel = mesh.get_relative_location()
        row['mesh_rel'] = [round(rel.x, 1), round(rel.y, 1), round(rel.z, 1)]
        rb = mesh.get_bone_location('root', unreal.BoneSpaces.COMPONENT_SPACE) if hasattr(mesh, 'get_bone_location') else None
        if rb is not None:
            row['root_cs'] = [round(rb.x, 1), round(rb.y, 1), round(rb.z, 1)]
    except Exception:  # noqa: BLE001
        pass
    if tap:
        row['tap'] = tap
    st['rows'].write(json.dumps(row) + '\n')


def _finish(status):
    _log(f'finish: {status}')
    st['phase'] = 'ending'
    st['status'] = status
    try:
        _console('t.OverrideFPS 0')
        g.release(0, 'move')
        ds.defence_reset()
        g.hit_rules_setup()
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
            _log(f'setup_editor: {g.setup_editor(True, "listen", 2)}')
            les.editor_request_begin_play()
            st.update(phase='wait_ready', t=now)
        elif phase == 'wait_ready':
            if les.is_in_play_in_editor() and _ready():
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready after 180 s')
        elif phase == 'settle' and now - st['t'] > 12:
            st['hkey'], st['akey'] = g.key_of_player(0), g.key_of_player(ATTACKER)
            _log(f'review stage: {_clear_stage()}; host {st["hkey"]}, attacker {st["akey"]}')
            g.hit_rules_setup()
            # The saved content kit, applied directly: defence_setup() fell back to the runtime path on KIT_Test_Hits in the first run (no roll back)
            kit = KIT
            if not ds._has_steps_saved():
                ds.Lib.add_defence_steps_to_rules(g.hit_rules())
                ds._saved['steps'] = True
            ds._saved['kit'] = kit
            _log(f'defence steps saved in the rules: {ds._has_steps_saved()}')
            _log(f'kits {kit}: host {g._apply_loadout(st["hkey"], kit)}, attacker {g._apply_loadout(st["akey"], kit)}')
            mesh = _mesh()
            comps = [c.get_class().get_name() for c in _pawn().get_components_by_class(unreal.ActorComponent) if 'Reaction' in c.get_class().get_name()]
            _log(f'anim mesh {mesh.get_name() if mesh else None}, reaction components {comps}')
            # Optional variant reaction set for the host only (in memory, nothing saved), e.g. a copy with new clips under review
            if os.environ.get('PIE_CAPTURE_REACTION_SET'):
                vset = unreal.load_asset(os.environ['PIE_CAPTURE_REACTION_SET'])
                for c in _pawn().get_components_by_class(unreal.GSTN_ReactionAnimComponent):
                    try:
                        c.set_reaction_set(vset)
                        _log(f'reaction set on the host: {vset.get_name() if vset else None}')
                    except Exception as ex:
                        _log(f'reaction set swap failed: {ex}')
            st.update(phase='cameras', t=now)
        elif phase == 'cameras':
            if _make_cameras() and _make_lights():
                st.update(phase='seq_prep', t=now)
            elif now - st['t'] > 20:
                _finish('could not summon the scene captures')
        elif phase == 'seq_prep' and now - st['t'] > 1.5:
            if st['seq'] >= len(SEQUENCES):
                return _finish('ok')
            if _seq_prep(SEQUENCES[st['seq']]):
                st.update(phase='seq_turn', t=now)
            else:
                _log('host still downed: jump to get up before the next sequence')
                st.update(t=now)
        elif phase == 'seq_turn' and now - st['t'] > 1.5:
            _turn_to_attacker(SEQUENCES[st['seq']].get('behind', False), SEQUENCES[st['seq']].get('side', 0))
            st.update(phase='seq_face', t=now)
        elif phase == 'seq_face' and SEQUENCES[st['seq']].get('behind') and now - st['t'] > 0.05:
            # Behind: stop on distance and heading, not wall time (react-hook-4: under load 0.3 s walked 155 cm, the victim stood 320 cm away,
            # out of the test move's reach, and the hit missed)
            loc, yaw = _pawn().get_actor_location(), _pawn().get_actor_rotation().yaw
            alo = g.snap(_world())[st['akey']]['location']
            away = math.degrees(math.atan2(loc.y - alo[1], loc.x - alo[0]))
            turned = abs((yaw - away + 180.0) % 360.0 - 180.0) < 15.0
            if (turned and math.hypot(loc.x - alo[0], loc.y - alo[1]) >= BEHIND_STOP) or now - st['t'] > 3.0 * FACE_SECONDS:
                g.release(0, 'move')
                st.update(phase='seq_begin', t=now)
        elif phase == 'seq_face' and SEQUENCES[st['seq']].get('side') and now - st['t'] > 0.05:
            yaw = _pawn().get_actor_rotation().yaw
            turned = abs((yaw - st['side_head'] + 180.0) % 360.0 - 180.0) < 12.0
            if turned or now - st['t'] > 3.0 * FACE_SECONDS:
                g.release(0, 'move')
                st.update(phase='seq_begin', t=now)
        elif phase == 'seq_face' and not SEQUENCES[st['seq']].get('behind') and not SEQUENCES[st['seq']].get('side') and now - st['t'] > FACE_SECONDS:
            g.release(0, 'move')
            st.update(phase='seq_begin', t=now)
        elif phase == 'seq_begin' and now - st['t'] > 2.5:
            seq = SEQUENCES[st['seq']]
            st['yaw'], st['aim'], st['run'] = _pawn().get_actor_rotation().yaw, None, {}
            _place_lights()
            loc = _pawn().get_actor_location()
            alo = g.snap(_world())[st['akey']]['location']
            a = g.snap(_world())[st['akey']]
            dist = math.hypot(loc.x - alo[0], loc.y - alo[1])
            facing_err = abs((st['yaw'] - (a['yaw'] + 180.0) + 180.0) % 360.0 - 180.0)
            _log(f'sequence {seq["name"]}: host at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}) yaw {st["yaw"]:.0f}, attacker at {[round(v) for v in alo]} '
                 f'yaw {a["yaw"]:.0f}, distance {dist:.0f} cm, facing error {facing_err:.0f} deg (0 = face to face), states {_states()}')
            os.makedirs(os.path.join(OUT, seq['name']), exist_ok=True)
            st['rows'] = open(os.path.join(OUT, seq['name'], 'frames.jsonl'), 'w')
            for cv, value in seq.get('cvars', {}).items():
                _console(f'{cv} {value}')
                _log(f'{seq["name"]}: {cv} {value}')
            _console('t.OverrideFPS 60')
            st.update(phase='seq_run', frame=0)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            states = _states()
            if not SHOOT_FROM_INPUT or ('input_at' in st['run'] and frame >= st['run']['input_at'] - 10):
                _shoot(seq, frame)
            elif 'down_at' in st['run']:
                _aim_cameras()   # the deadzone aim keeps following the flight, so the first shot frames the pawn
            tap, done = _step(seq, frame, states)
            _row(frame, states, tap)
            if tap:
                _log(f'{seq["name"]}: v{frame} {tap} (states {states})')
            st['frame'] = frame + 1
            if done:
                st['rows'].close()
                for cv in seq.get('cvars', {}):
                    _console(f'{cv} -1')
                _console('t.OverrideFPS 0')
                g.release(0, 'move')
                _log(f'sequence {seq["name"]} done: {st["frame"]} frames, run {st["run"]}')
                st.update(phase='seq_prep', seq=st['seq'] + 1, t=now)
        elif phase == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:
                _log(f'setup_editor(False) failed: {e!r}')
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write(st.get('status', '?') + '\n')
            if any(s.get('follow') for s in SEQUENCES):
                with open(os.path.join(OUT, 'assert.txt'), 'w') as f:
                    f.write(_assert_lines() + '\n')
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


# NIGHT-ABP experiments: console variables set before Play (';' separated), e.g. a.AnimNode.OffsetRootBone.Enable 0
for _cv in [c.strip() for c in os.environ.get('PIE_CAPTURE_CVARS', '').split(';') if c.strip()]:
    unreal.SystemLibrary.execute_console_command(None, _cv)
    _log(f'cvar: {_cv}')
# getup-momentum-1: PIE_CAPTURE_GETUP_SETUP=1 writes the get-up FAF, tail and exit speeds of setup_reaction_anims.GETUPS into the ability
# classes IN MEMORY before Play (nothing saved: the Blueprints change only once the owner OKs it)
if os.environ.get('PIE_CAPTURE_GETUP_SETUP') == '1':
    os.environ['REACTION_ONLY_GETUPS'], os.environ['REACTION_NO_SAVE'] = '1', '1'
    sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GAS'))
    import setup_reaction_anims as _sra   # noqa: E402
    for _line in _sra.run():
        _log(f'getup setup (memory): {_line}')
_log(f'driver loaded: out {OUT}, sequences {[s["name"] for s in SEQUENCES]}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
