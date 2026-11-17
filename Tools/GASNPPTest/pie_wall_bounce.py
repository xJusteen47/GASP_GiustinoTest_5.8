# WALL-BOUNCE-1: the default wall bounce / bump (CombatRules WallImpact, 2026-11-17) checked in Play, plus the bump reaction rules in the reaction set.
# 1) Edits /Game/GASNPPTest/Combat/Reaction/DA_ReactionAnims (adds or updates BumpWall / BumpCeiling: RequiredTags Combat.State.Bump.Wall / .Ceiling,
#    Priority = highest Tumble rule + 1, EndBehaviour BlendOut, BlendIn 2, BlendOut 6, Animation = the clip of a Hitstun rule as a placeholder) and
#    saves ONLY that asset.
# 2) Play in L_Test_Hub, listen host + 2 clients, t.OverrideFPS 60 (one tick = one frame). The host pawn is the victim, client 1 the attacker; a test
#    surface box (AGSTN_TestSurface, leaf Wall) is spawned IMPACT_FACE cm ahead of the attacker, no impact table (so the default WallImpact decides).
#    Runs: weak (victim at WEAK_DAMAGE %: a launch well under 1200 cm/s into the face -> bump) and strong (STRONG_DAMAGES tried in order until the
#    judged impact speed is over 1200 cm/s -> hard bounce, no Bump tag).
# Output (OUT, default Saved/WallBounce/<date_time>): <run>/side/fNNN.png, <run>/frames.jsonl (f, tags, vel, loc, wall distance, montage, impact info),
# capture.log, done.txt. Launch: Run-Timed.ps1 -Preset Capture -Script <boot.py without spaces> that exec()s this file.
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

OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'WallBounce',
                                                        time.strftime('%Y%m%d_%H%M%S'))
MAP = '/Game/Levels/Test/L_Test_Hub'
SET = '/Game/GASNPPTest/Combat/Reaction/DA_ReactionAnims'
SIZE = 480
DISTANCE, ELEVATION, FOV = 520.0, 8.0, 50.0
ATTACKER = 1
PLACE = 205.0          # the victim ahead of the attacker (the test move's Root box reach)
IMPACT_FACE = 455.0    # the face of the wall ahead of the attacker
WALL_HALF = (50.0, 500.0, 500.0)
WEAK_DAMAGE = float(os.environ.get('WALL_WEAK_DAMAGE') or 0.0)   # run1: 80 % gave 1552 cm/s into the wall (hard bounce)
WEAK_FACE = 335.0     # run2: at 0 % the victim flew 432 cm/s but landed 77 cm before a face at 455: the weak run's wall is closer
STRONG_DAMAGES = [250.0, 400.0, 700.0]
PRE, POST = 10, 110    # frames before the attack tap, frames after it
TAGS = ['Hitstun', 'Tumble', 'Hitstop', 'Bump', 'Bump.Wall', 'Bump.Ceiling', 'Splat', 'Knockdown']
TIMEOUT = 780.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'cam': None, 'rt': None, 'lights': [], 'handle': None, 'runs': [], 'ri': 0,
      'strong_i': 0}


def _log(msg):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'capture.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {msg}\n')


# ---- 1) the reaction set ---------------------------------------------------------------------------------------------------------------------
def _tag_names(container):
    txt = container.export_text() if hasattr(container, 'export_text') else str(container)
    out, key = [], 'TagName="'
    i = txt.find(key)
    while i >= 0:
        j = txt.find('"', i + len(key))
        out.append(txt[i + len(key):j])
        i = txt.find(key, j)
    return out


def _container(tag):
    c = unreal.GameplayTagContainer()
    c.import_text(f'(GameplayTags=((TagName="{tag}")))')
    return c


def edit_reaction_set():
    da = unreal.load_asset(SET)
    rules = list(da.get_editor_property('rules'))
    tumble_pri, hitstun = None, []
    for r in rules:
        tags = _tag_names(r.get_editor_property('required_tags'))
        pri = r.get_editor_property('priority')
        if 'Combat.State.Tumble' in tags:
            tumble_pri = pri if tumble_pri is None else max(tumble_pri, pri)
        if 'Combat.State.Hitstun' in tags and 'Combat.State.Tumble' not in tags and r.get_editor_property('animation'):
            hitstun.append(r)
        _log(f'rule {r.get_editor_property("name")} pri {pri} tags {tags} anim {r.get_editor_property("animation")}')
    if tumble_pri is None or not hitstun:
        return f'cannot edit: tumble priority {tumble_pri}, hitstun rules with a clip {len(hitstun)}'

    def anim_name(r):
        a = r.get_editor_property('animation')
        return a.get_name() if a else ''
    front = [r for r in hitstun if any(k in anim_name(r).lower() for k in ('front', 'fwd', 'forward', '_f_', '_f'))]
    src = (front or hitstun)[0]
    anim = src.get_editor_property('animation')
    for name, tag in (('BumpWall', 'Combat.State.Bump.Wall'), ('BumpCeiling', 'Combat.State.Bump.Ceiling')):
        rule = next((r for r in rules if str(r.get_editor_property('name')) == name), None)
        new = rule is None
        if new:
            rule = unreal.GSTN_ReactionAnimRule()
        rule.set_editor_property('name', name)
        rule.set_editor_property('required_tags', _container(tag))
        rule.set_editor_property('priority', tumble_pri + 1)
        rule.set_editor_property('animation', anim)
        rule.set_editor_property('end_behaviour', unreal.GSTN_ReactionAnimEnd.BLEND_OUT)
        rule.set_editor_property('blend_in_frames', 2)
        rule.set_editor_property('blend_out_frames', 6)
        if new:
            rules.append(rule)
        else:
            rules = [rule if str(r.get_editor_property('name')) == name else r for r in rules]
    da.set_editor_property('rules', rules)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(da, False)
    check = [(str(r.get_editor_property('name')), r.get_editor_property('priority'), _tag_names(r.get_editor_property('required_tags')))
             for r in unreal.load_asset(SET).get_editor_property('rules') if str(r.get_editor_property('name')).startswith('Bump')]
    return (f'placeholder {anim.get_path_name()} (from rule {src.get_editor_property("name")}), priority {tumble_pri + 1} '
            f'(tumble max {tumble_pri}), saved {saved}, rules now {check}')


# ---- 2) Play ---------------------------------------------------------------------------------------------------------------------------------
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


def _make_camera():
    if st['cam']:
        return True
    found = list(unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.SceneCapture2D))
    if not found:
        _console('Summon SceneCapture2D')
        return False
    st['cam'] = found[0]
    comp = st['cam'].capture_component2d
    st['rt'] = unreal.RenderingLibrary.create_render_target2d(_world(), SIZE * 16 // 9, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    for prop, value in (('capture_every_frame', False), ('capture_on_movement', False), ('always_persist_rendering_state', True),
                        ('fov_angle', FOV), ('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR), ('texture_target', st['rt'])):
        comp.set_editor_property(prop, value)
    return True


def _make_lights():
    if st['lights']:
        return True
    if st.get('lights_before') is None:
        st['lights_before'] = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.DirectionalLight)}
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.DirectionalLight) if a.get_name() not in st['lights_before']]
    if len(new) < 2:
        _console('Summon DirectionalLight')
        return False
    st['lights'] = new[:2]
    for light, intensity in zip(st['lights'], (5.0, 3.0)):
        comp = light.get_component_by_class(unreal.DirectionalLightComponent)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_intensity(intensity)
        comp.set_cast_shadows(False)
    return True


def _make_wall_visual():
    """The test surface is a collision box only (nothing drawn): a summoned cube, no collision, stands in for it in the captures"""
    if st.get('visual'):
        return True
    if st.get('sm_before') is None:
        st['sm_before'] = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.StaticMeshActor)}
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.StaticMeshActor) if a.get_name() not in st['sm_before']]
    if not new:
        _console('Summon StaticMeshActor')
        return False
    a = new[0]
    comp = a.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    st['visual'] = a
    return True


def _has(name):
    return ds.has_tag(0, st['hkey'], 'Combat.State.' + name)


def _montage():
    try:
        mesh = _pawn().get_component_by_class(unreal.SkeletalMeshComponent)
        anim = mesh.get_anim_instance()
        m = anim.get_current_active_montage()
        return [m.get_name(), round(anim.montage_get_position(m) * 60.0, 1)] if m else None
    except Exception as e:  # noqa: BLE001
        return ['error', repr(e)[:60]]


def _prep(run):
    """Attacker on a clear spot, the wall ahead of it, the victim PLACE cm ahead, no impact table, damage set"""
    akey, hkey = st['akey'], st['hkey']
    ctx = g.comps(_world())[akey]
    lib = unreal.GSTN_ImpactTestLibrary
    lib.clear_test_surfaces(ctx)
    g.combat_clear(ATTACKER)
    g.hit_rules_setup()
    g.set_tuning(push_back_threshold=50.0, launch_threshold=100.0, tumble_threshold=200.0)
    lib.configure_impact_table('')
    lib.set_hit_impact_profile(0, 0, False, 1.0)
    if st.get('spot') is None:
        st['spot'] = g._impact_clear_spot(akey)
        _log(f'attacker clear spot {st["spot"]}')
    if st['spot'] is not None:
        ctx.server_reset_to_spawn(unreal.Vector(st['spot'][0], st['spot'][1], 0.0))
    return True


def _place(run):
    akey, hkey = st['akey'], st['hkey']
    ctx = g.comps(_world())[akey]
    a = g.snap(_world())[akey]
    yaw = a['yaw']
    fwd = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
    face = run.get('face', IMPACT_FACE)
    centre = face + WALL_HALF[0]
    c = (a['location'][0] + fwd[0] * centre, a['location'][1] + fwd[1] * centre, a['location'][2] + 200.0)
    surf = unreal.GSTN_ImpactTestLibrary.spawn_test_surface(ctx, unreal.Vector(*c), unreal.Vector(*WALL_HALF), yaw, 'Wall')
    vis = st['visual']
    vis.set_actor_location_and_rotation(unreal.Vector(*c), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False, True)
    vis.set_actor_scale3d(unreal.Vector(WALL_HALF[0] / 50.0, WALL_HALF[1] / 50.0, WALL_HALF[2] / 50.0))
    g._place_near(ATTACKER, hkey, (PLACE, 0.0))
    g.set_damage(hkey, run['damage'])
    st['wall'] = {'origin': (a['location'][0] + fwd[0] * face, a['location'][1] + fwd[1] * face), 'fwd': fwd, 'yaw': yaw}
    _log(f'run {run["name"]}: damage {run["damage"]}, attacker at {[round(v) for v in a["location"]]} yaw {yaw:.0f}, surface {surf}')


def _aim():
    """Side view: perpendicular to the flight, centred between the victim's start and the wall face"""
    w = st['wall']
    loc = _pawn().get_actor_location()
    fx, fy = w['fwd']
    cx = w['origin'][0] - fx * 120.0
    cy = w['origin'][1] - fy * 120.0
    centre = unreal.Vector(cx, cy, loc.z + 40.0)
    side = (-fy, fx)
    e = math.radians(ELEVATION)
    pos = unreal.Vector(cx + side[0] * DISTANCE * math.cos(e), cy + side[1] * DISTANCE * math.cos(e), centre.z + DISTANCE * math.sin(e))
    st['cam'].set_actor_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, centre), False, False)
    for light, az in zip(st['lights'], (35.0, 215.0)):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-30.0, yaw=w['yaw'] + 90.0 + az), False)


def _shoot(run, frame):
    _aim()
    hidden = [p for p in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Pawn) if p != _pawn()]
    st['cam'].capture_component2d.set_editor_property('hidden_actors', hidden)
    st['cam'].capture_component2d.capture_scene()
    folder = os.path.join(OUT, run['name'], 'side')
    os.makedirs(folder, exist_ok=True)
    unreal.RenderingLibrary.export_render_target(_world(), st['rt'], folder, f'f{frame:03d}.png')


DIAG = os.environ.get('WALL_DIAG') == '1'   # round 2: why the BumpWall montage stops one frame after it starts


def _diag_hook():
    """Montage started / blending out / ended events of the victim's anim instance, stamped with the capture frame"""
    if st.get('diag_hooked'):
        return
    st['diag_hooked'], st['events'], st['seen'] = True, [], {}
    anim = _pawn().get_component_by_class(unreal.SkeletalMeshComponent).get_anim_instance()
    st['anim'] = anim

    def ev(kind):
        def f(montage, *args):
            st['seen'][montage.get_name()] = montage
            st['events'].append([st.get('frame'), kind, montage.get_name(), [str(a) for a in args]])
        return f
    for name, kind in (('on_montage_started', 'start'), ('on_montage_blending_out', 'blendout'), ('on_montage_ended', 'end')):
        try:
            getattr(anim, name).add_callable(ev(kind))
        except Exception as e:  # noqa: BLE001
            _log(f'diag: cannot bind {name}: {e!r}')
    try:
        st['slot'] = str(unreal.load_asset(SET).get_editor_property('slot_name'))
        clip = unreal.load_asset('/Game/GASNPPTest/Combat/Reaction/AS_React_HitStand')
        _log(f'diag: slot {st["slot"]}, AS_React_HitStand length {clip.get_play_length():.3f} s = {clip.get_play_length() * 60:.1f} frames')
    except Exception as e:  # noqa: BLE001
        _log(f'diag: set/clip read failed {e!r}')


def _diag(frame):
    anim, out = st['anim'], {}
    mons = []
    for name, m in st['seen'].items():
        try:
            if anim.montage_is_active(m) or anim.montage_is_playing(m):
                grp = ''
                try:
                    grp = str(m.get_group_name())
                except Exception:  # noqa: BLE001
                    pass
                mons.append([name, grp, round(anim.montage_get_position(m) * 60.0, 1), bool(anim.montage_is_playing(m)),
                             bool(anim.montage_get_is_stopped(m))])
        except Exception as e:  # noqa: BLE001
            mons.append([name, 'err', repr(e)[:50]])
    out['montages'] = mons
    try:
        out['slot_w'] = round(anim.get_slot_montage_global_weight(st.get('slot', 'DefaultSlot')), 3)
    except Exception as e:  # noqa: BLE001
        out['slot_w'] = repr(e)[:40]
    try:
        rc = _pawn().get_component_by_class(unreal.GSTN_ReactionAnimComponent)
        out['rule'] = str(rc.get_active_rule_name())
    except Exception as e:  # noqa: BLE001
        out['rule'] = repr(e)[:40]
    asc = _pawn().get_component_by_class(unreal.AbilitySystemComponent)
    try:
        out['all_tags'] = sorted(t for t in str(asc.get_owned_gameplay_tags()).replace('"', ' ').split() if '.' in t and t[0].isupper())
    except Exception:  # noqa: BLE001
        try:
            out['all_tags'] = _tag_names(unreal.BlueprintGameplayTagLibrary.get_owned_gameplay_tags(_pawn()))
        except Exception as e:  # noqa: BLE001
            out['all_tags'] = repr(e)[:40]
    try:
        m = asc.get_current_montage()
        out['asc_montage'] = m.get_name() if m else None
    except Exception as e:  # noqa: BLE001
        out['asc_montage'] = repr(e)[:40]
    out['events'] = [e for e in st['events'] if e[0] == frame]
    return out


def _row(run, frame, tap):
    v = g.snap(_world())[st['hkey']]
    loc, vel = v['location'], v.get('velocity', (0, 0, 0))
    w = st['wall']
    dist = (w['origin'][0] - loc[0]) * w['fwd'][0] + (w['origin'][1] - loc[1]) * w['fwd'][1]
    cc = g._hit_comp(0, st['hkey'])
    row = {'f': frame, 'tags': [t for t in TAGS if _has(t)], 'vel': [round(x) for x in vel],
           'vin': round(vel[0] * w['fwd'][0] + vel[1] * w['fwd'][1]), 'loc': [round(x, 1) for x in loc], 'wall_cm': round(dist, 1),
           'montage': _montage(), 'impact': cc.get_impact_info(), 'counts': cc.get_impact_counts(), 'mode': v.get('mode')}
    if DIAG:
        row['diag'] = _diag(frame)
    if tap:
        row['tap'] = tap
    run['rows'].append(row)
    st['fh'].write(json.dumps(row) + '\n')


def _summary(run):
    rows = run['rows']
    contact = next((r['f'] for r in rows if r['wall_cm'] <= 45.0), None)
    bump = [r['f'] for r in rows if 'Bump.Wall' in r['tags']]
    after = [r for r in rows if contact is not None and r['f'] >= contact]
    max_off = max((r['wall_cm'] for r in after), default=None)
    mont = sorted({r['montage'][0] for r in after if r['montage']})
    vin_before = next((rows[i - 1]['vin'] for i, r in enumerate(rows) if i and r['f'] == contact), None)
    s = {'run': run['name'], 'damage': run['damage'], 'contact_frame': contact, 'speed_into_wall_before': vin_before,
         'impact_info_last': rows[-1]['impact'], 'counts_last': rows[-1]['counts'],
         'bump_frames': [bump[0], bump[-1], len(bump)] if bump else None, 'max_wall_cm_after_contact': max_off, 'montages_after_contact': mont,
         'tags_at_contact': next((r['tags'] for r in rows if r['f'] == contact), None)}
    with open(os.path.join(OUT, run['name'], 'summary.json'), 'w') as f:
        json.dump(s, f, indent=1)
    _log(f'summary {json.dumps(s)}')
    return s


def _finish(status):
    _log(f'finish: {status}')
    st['phase'] = 'ending'
    st['status'] = status
    try:
        _console('t.OverrideFPS 0')
        unreal.GSTN_ImpactTestLibrary.configure_impact_table('')
        g.hit_rules_setup()
    except Exception:  # noqa: BLE001
        pass
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()


def _next_run():
    if st['ri'] == 0:
        return {'name': 'weak', 'damage': WEAK_DAMAGE, 'face': WEAK_FACE, 'rows': []}
    return {'name': 'strong', 'damage': STRONG_DAMAGES[st['strong_i']], 'rows': []}


def _tick(_dt):
    try:
        now, phase = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase != 'ending' and now - st['start'] > TIMEOUT:
            return _finish(f'timeout in phase {phase}')
        if phase == 'boot' and now - st['t'] > 8:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if world is None or world.get_name() != 'L_Test_Hub':
                _log(f'loading {MAP} (was {world.get_name() if world else None}): {les.load_level(MAP)}')
            st.update(phase='edit', t=now)
        elif phase == 'edit' and now - st['t'] > 4:
            if os.environ.get('WALL_SKIP_EDIT') != '1':
                _log('reaction set: ' + edit_reaction_set())
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
            parked = [h.park(i, 'Z1') for i in h._players() if i not in (0, ATTACKER)]
            _console('gstn.Combat.KitPlaceholderLook 0')
            _log(f'host {st["hkey"]}, attacker {st["akey"]}, parked {parked}, kit {g._apply_loadout(st["akey"], g.HIT_KIT)}')
            st.update(phase='cameras', t=now)
        elif phase == 'cameras':
            if _make_camera() and _make_lights() and _make_wall_visual():
                if DIAG:
                    _diag_hook()
                st.update(phase='prep', t=now)
            elif now - st['t'] > 20:
                _finish('could not summon the scene capture')
        elif phase == 'prep' and now - st['t'] > 1.5:
            if st['ri'] >= (1 if os.environ.get('WALL_ONLY_WEAK') == '1' else 2):
                return _finish('ok')
            st['run'] = _next_run()
            _prep(st['run'])
            st.update(phase='place', t=now)
        elif phase == 'place' and now - st['t'] > 1.5:
            _place(st['run'])
            st.update(phase='begin', t=now)
        elif phase == 'begin' and now - st['t'] > 2.5:
            run = st['run']
            os.makedirs(os.path.join(OUT, run['name']), exist_ok=True)
            st['fh'] = open(os.path.join(OUT, run['name'], 'frames.jsonl'), 'w')
            _console('t.OverrideFPS 60')
            st.update(phase='run', frame=0)
        elif phase == 'run':
            run, frame = st['run'], st['frame']
            tap = None
            if frame == PRE:
                g.combat_tap(ATTACKER, g.button_mask('Attack'))
                tap = 'attack c1'
            _shoot(run, frame)
            _row(run, frame, tap)
            st['frame'] = frame + 1
            if frame >= PRE + POST:
                st['fh'].close()
                _console('t.OverrideFPS 0')
                s = _summary(run)
                speed = s['speed_into_wall_before'] or 0
                if run['name'] == 'strong' and speed < 1250 and st['strong_i'] + 1 < len(STRONG_DAMAGES):
                    _log(f'strong run at {run["damage"]} % only {speed} cm/s into the wall: again with more damage')
                    st['strong_i'] += 1
                else:
                    st['ri'] += 1
                st.update(phase='prep', t=now)
        elif phase == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:  # noqa: BLE001
                _log(f'setup_editor(False) failed: {e!r}')
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write(st.get('status', '?') + '\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:  # noqa: BLE001
        _log('ERROR ' + traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error (see capture.log)')
        else:
            unreal.unregister_slate_post_tick_callback(st['handle'])
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write('error while ending\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    return None


_log(f'driver loaded: out {OUT}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
