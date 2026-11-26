# Owner playtest 2026-11-25: the Overlord gets four reference moves with Smash Ultimate's numbers (the fixed targets of different hardness to
# hit them with are another thread's: gstn.TestCubes.Spawn, branch claude/project-thread-k3aapg). Numbers, sources and conversions: Tools/GAS/overlord_ref_moves_data.py (pure Python) and
# docs/notes/overlord-ref-moves.md. Greybox phase: every pose is a capsule placeholder made from numbers, no final animation is touched.
#
#   F.S        Command.Strong.Directional, grounded  -> GA_Overlord_RefFSGanon     (Ganondorf forward smash)
#   N.S        Command.Strong.Neutral,     grounded  -> GA_Overlord_RefNSIke       (Ike forward tilt)
#   N.L Aerial Command.Attack.Neutral,     airborne  -> GA_Overlord_RefNLAirGanon  (Ganondorf neutral aerial)
#   N.S Aerial Command.Strong.Neutral,     airborne  -> GA_Overlord_RefNSAirIke    (Ike neutral aerial)
#
# Writes (new assets; a rerun rewrites its own, setup_guard keeps hand-edited timelines):
#   /Game/GSTN/Greybox/Clips/AS_GSTN_Greybox_Ref_<Move>   capsule blocking clip (mannequin skeleton, 60 fps, key poses below), rebuilt in place
#   Characters/Overlord/Moves/Ref/AM_OverlordRef_<Move>    montage of that clip (no blend in, 4 frame blend out)
#   Characters/Overlord/Moves/Ref/TL_Overlord_Ref<Move>    UGSTN_CombatTimeline: frames, hitbox paths, hits (Smash numbers), penetration,
#                                                          ground rule, aerial landing windows (autocancel / landing lag)
#   Characters/Overlord/Moves/Ref/GA_Overlord_Ref<Move>    thin Blueprint child of UGSTN_GA_TestKitStrike whose TimelineAsset is the timeline
# Changes (backed up to Saved/Backups/GAS/<time>/ first):
#   KIT_Test_Overlord (or REF_MOVES_KITS)   kit bindings: an existing kit binding of the same command and context (grounded = does not require
#                                           Input.Context.Airborne, airborne = requires it; never one keyed on a link tag) gets the new ability in
#                                           place; with none, a new binding goes FIRST (the kit's bindings come before its weapons'). New bindings
#                                           ignore Combat.Link.Linked, so a grab's own bindings (pummel, throws) keep winning while linked.
#   ABP_GSTN_GreyboxCapsule                 MoveOverrides: montage -> its own clip, merged (REF_MOVES_CAPSULE=0 skips this shared Blueprint).
#                                           Tools/GAS/greybox/make_capsule_clips.py keeps these entries on its own runs (montages that already
#                                           play a greybox clip map to that clip).
#
# Headless (editor closed):  .\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\setup_overlord_ref_moves.py
# Editor:                    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_overlord_ref_moves as s; s.run()
# Output: Saved/OverlordRefMoves/setup_overlord_ref_moves.txt, at most 5 printed lines.
import datetime
import os
import shutil
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (HERE, os.path.join(HERE, 'greybox')):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import setup_guard  # noqa: E402
import gstn_content as gc  # noqa: E402
import overlord_ref_moves_data as D  # noqa: E402
import make_capsule_clips as mcc  # noqa: E402  (the capsule pose language and the mannequin rig; importing it runs nothing)

DIR = gc.move_folder('Ref')
CLIP_DIR = mcc.DIR
KITS = [k.strip() for k in os.environ.get('REF_MOVES_KITS', 'KIT_Test_Overlord').split(',') if k.strip()]
CAPSULE = os.environ.get('REF_MOVES_CAPSULE', '1') != '0'
AIRBORNE, LINKED = 'Input.Context.Airborne', 'Combat.Link.Linked'
BLEND_OUT_SECONDS = 4.0 / 60.0
SAKURAI_ANGLE_DEG = 40.0   # the angle Sakurai3D starts from in the air (Ultimate's aerial Sakurai angle is about 40)
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'OverlordRefMoves', 'setup_overlord_ref_moves.txt')
BACKUP = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'GAS', datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
_lines = []
_guard = None

# ---- capsule key poses (pose language of make_capsule_clips: lean, drop, fwd, twist, arm_l/r, out_l/r, elbow_l/r, leg_l/r, knee_l/r...)
# Frames are Smash frames at 60 fps (frame 0 = the press). Placeholders for the greybox capsule, judged by the owner in game (rule 6).
TWO_HANDS_UP = {'lean': -12, 'drop': 4, 'arm_l': 165, 'arm_r': 165, 'out_l': -10, 'out_r': -10, 'elbow_l': 20, 'elbow_r': 20, 'head': -10}
CHOP_DOWN = {'lean': 30, 'drop': 14, 'fwd': 12, 'arm_l': 35, 'arm_r': 35, 'out_l': -15, 'out_r': -15, 'elbow_l': 5, 'elbow_r': 5,
             'leg_l': 35, 'knee_l': 30, 'leg_r': -20, 'knee_r': 20}
# Centred (owner 2026-11-25): the chest turns +45 -> 0 at the middle of the active frames (12.5) -> -45, the striking arm straight ahead at 0
SWING_BACK = {'twist': 45, 'lean': -5, 'drop': 4, 'arm_r': 50, 'out_r': 70, 'elbow_r': 30, 'arm_l': 20, 'elbow_l': 60}
SWING_MID = {'twist': 0, 'lean': 12, 'drop': 8, 'fwd': 14, 'arm_r': 90, 'out_r': 0, 'elbow_r': 0, 'arm_l': 0, 'out_l': 25, 'elbow_l': 40,
             'leg_r': 40, 'knee_r': 30, 'leg_l': -20, 'knee_l': 15}
SWING_OUT = {'twist': -45, 'lean': 18, 'drop': 10, 'fwd': 18, 'arm_r': 90, 'out_r': 5, 'elbow_r': 0, 'arm_l': -20, 'out_l': 30,
             'leg_r': 45, 'knee_r': 35, 'leg_l': -25, 'knee_l': 15}
AIR = mcc.AIR_A
KICK_1 = dict(AIR, lean=-12, leg_r=85, knee_r=10, leg_l=20, knee_l=80, arm_l=-20, arm_r=-20, out_l=50, out_r=50)
KICK_2 = dict(AIR, lean=-22, leg_l=95, knee_l=5, leg_r=10, knee_r=90, twist=-20, arm_l=-30, arm_r=-30, out_l=60, out_r=60)
SWORD_UP = dict(AIR, arm_r=170, out_r=0, elbow_r=0, arm_l=40, lean=-10)
SWORD_FRONT = dict(AIR, arm_r=80, elbow_r=0, lean=10)
SWORD_DOWN = dict(AIR, arm_r=-10, out_r=10, elbow_r=0, lean=20, leg_l=60, leg_r=60)
SWORD_BACK = dict(AIR, arm_r=-80, out_r=15, elbow_r=0, lean=5, twist=25)
POSES = {
    'FS': [(0, mcc.IDLE), (14, TWO_HANDS_UP), (27, dict(TWO_HANDS_UP, lean=-16)), (31, CHOP_DOWN), (52, CHOP_DOWN), (70, mcc.IDLE), (90, mcc.IDLE)],
    'NS': [(0, mcc.IDLE), (8, SWING_BACK), (12, SWING_MID), (15, SWING_OUT), (26, SWING_OUT), (42, mcc.IDLE), (60, mcc.IDLE)],
    'NLAir': [(0, AIR), (5, dict(AIR, leg_r=40, knee_r=110)), (8, KICK_1), (13, KICK_1), (15, dict(AIR, leg_l=40, knee_l=110)), (18, KICK_2),
              (27, KICK_2), (41, AIR), (55, AIR)],
    'NSAir': [(0, AIR), (8, SWORD_UP), (12, SWORD_FRONT), (15, SWORD_DOWN), (19, SWORD_BACK), (24, dict(SWORD_BACK, arm_r=-140)), (40, AIR),
              (68, AIR)],
}


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_ref_moves] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _container(names):
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_tag(n) for n in names])


def _names(container):
    return [str(t.get_editor_property('tag_name')) for t in unreal.GameplayTagLibrary.break_gameplay_tag_container(container)]


def _tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _backup(asset_path):
    src = setup_guard.asset_file(os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())), asset_path)
    if os.path.exists(src):
        os.makedirs(BACKUP, exist_ok=True)
        shutil.copy2(src, os.path.join(BACKUP, os.path.basename(src)))


# ---- clip and montage
def _clip(rig, key):
    """The capsule clip, rebuilt IN PLACE when it exists (the montage keeps pointing at it; make_capsule_clips deletes and remakes its own)"""
    keys = POSES[key]
    frames = D.MOVES[key]['total']
    name = f'AS_GSTN_Greybox_Ref_{D.ASSET[key]}'
    path = f'{CLIP_DIR}/{name}'
    seq = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not seq:
        factory = unreal.AnimSequenceFactory()
        factory.set_editor_property('target_skeleton', rig.skeleton)
        # AnimSequenceFactory calls NotifyPopulated(), which locks PlatformTargetFrameRate to AnimationSettings.DefaultFrameRate (30 fps).
        # Set it to ANIM_FPS (60) around creation so odd 60 fps lengths (e.g. N.L Aerial = 55 frames) do not resample to 30 fps with a 0.5 sub-frame.
        anim_settings = unreal.get_default_object(unreal.AnimationSettings)
        prev_fps = anim_settings.get_editor_property('default_frame_rate')
        anim_settings.set_editor_property('default_frame_rate', unreal.FrameRate(int(D.ANIM_FPS), 1))
        try:
            seq = _tools().create_asset(name, CLIP_DIR, unreal.AnimSequence, factory)
        finally:
            anim_settings.set_editor_property('default_frame_rate', prev_fps)
        log(f'created {path}')
    ctrl = seq.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('greybox ref move clip'), False)
    if hasattr(ctrl, 'remove_all_bone_tracks'):
        ctrl.remove_all_bone_tracks(False)
    ctrl.set_frame_rate(unreal.FrameRate(int(D.ANIM_FPS), 1), False)
    ctrl.set_number_of_frames(unreal.FrameNumber(frames), False)
    samples = frames + 1
    poses = [rig.own_world(mcc.pose_at(keys, f)) for f in range(samples)]
    pelvis = [rig.pelvis_translation(mcc.pose_at(keys, f)) for f in range(samples)]
    for bone in rig.names:
        lt = rig.loc[bone]
        rots = [unreal.Quat(*rig.local_rotation(bone, poses[f][bone])) for f in range(samples)] if bone in poses[0] else [lt.rotation] * samples
        trans = pelvis if bone == 'pelvis' else [lt.translation] * samples
        if hasattr(ctrl, 'add_bone_curve'):
            ctrl.add_bone_curve(bone, False)
        ctrl.set_bone_track_keys(bone, trans, rots, [unreal.Vector(1, 1, 1)] * samples, False)
    ctrl.close_bracket(False)
    seq.set_editor_property('enable_root_motion', False)
    return seq


def _montage(key, clip):
    name = f'AM_OverlordRef_{D.ASSET[key]}'
    path = f'{DIR}/{name}'
    montage = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', clip)
        factory.set_editor_property('target_skeleton', clip.get_editor_property('skeleton'))
        montage = _tools().create_asset(name, DIR, unreal.AnimMontage, factory)
        log(f'created {path}')
    for prop, seconds in (('blend_in', 0.0), ('blend_out', BLEND_OUT_SECONDS)):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', seconds)
        montage.set_editor_property(prop, blend)
    return montage


# ---- timeline
def _append_tracks(data, extra):
    """Tracks are instanced structs, which Python cannot build: the extra ones go in through the struct's text form (as setup_overlord_jab_kit)"""
    text = data.export_text()
    start = text.find('Tracks=(')
    if start < 0:
        raise RuntimeError('no Tracks in the timeline text: ' + text[:300])
    i, depth, quoted = start + len('Tracks=('), 1, False
    while depth and i < len(text):
        c = text[i]
        if c == '"' and text[i - 1] != '\\':
            quoted = not quoted
        elif not quoted:
            depth += (c == '(') - (c == ')')
        i += 1
    inner = text[start + len('Tracks=('):i - 1]
    data.import_text('(Tracks=(' + ','.join(([inner] if inner else []) + extra) + '))')


def _hit_data(key, hit):
    n = D.hit_numbers(key, hit)
    data = unreal.GSTN_HitData()
    sakurai = n['angle'] == 'sakurai'
    for prop, value in (('damage', n['damage']), ('base_knockback', n['bkb_cms']), ('knockback_growth', float(n['kbg'])),
                        ('angle_deg', SAKURAI_ANGLE_DEG if sakurai else float(n['angle'])),
                        ('special_angle', unreal.GSTN_SpecialAngle.SAKURAI3D if sakurai else unreal.GSTN_SpecialAngle.NONE),
                        ('hitstop_mode', unreal.GSTN_HitstopMode.OVERRIDE), ('hitstop_frames_override', n['hitstop']),
                        ('attacker_hitstop_frames_override', n['hitstop']), ('priority', n['priority'])):
        data.set_editor_property(prop, value)
    if hasattr(unreal, 'GSTN_PartHitData'):
        inst = unreal.InstancedStruct()
        inst.import_text(f'/Script/GSTNPartsCore.GSTN_PartHitData(Penetration={n["penetration"]})')
        data.set_editor_property('extension_data', [inst])
    return data


def _group(key, hit):
    return 1 if key == 'NLAir' and hit.startswith('K2') else 0


def _boxes(key, window):
    out = []
    boxes = D.window_keys(window)
    for index, (hit, radius, half, keys) in enumerate(boxes):
        shape = unreal.GSTN_HitShape()
        if half > 0.0:
            shape.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
            shape.set_editor_property('extent', unreal.Vector(radius, radius, half + radius))
        else:
            shape.set_editor_property('type', unreal.GSTN_ShapeType.SPHERE)
            shape.set_editor_property('extent', unreal.Vector(radius, radius, radius))
        ukeys = []
        for frame, centre, (pitch, yaw) in keys:
            k = unreal.GSTN_HitboxKey()
            k.set_editor_property('frame', frame)
            k.set_editor_property('offset', unreal.Vector(*centre))
            k.set_editor_property('rotation', unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
            ukeys.append(k)
        hit_data = _hit_data(key, hit)
        # Smash: the lowest hitbox ID wins an overlap. Here the higher Priority wins, so the first box of a window gets the highest
        hit_data.set_editor_property('priority', len(boxes) - index)
        spec = unreal.GSTN_HitboxSpec()
        for prop, value in (('shape', shape), ('space', unreal.GSTN_HitboxSpace.ROOT), ('hit_group', _group(key, hit)), ('data', hit_data),
                            ('keys', ukeys), ('sweep_between_frames', True)):
            spec.set_editor_property(prop, value)
        out.append(spec)
    return out


def _timeline(key, montage):
    m = D.MOVES[key]
    data = unreal.GSTN_CombatTimelineData()
    for prop, value in (('total_frames', m['total']), ('first_actionable_frame', m['faf']), ('montage', montage),
                        ('montage_play_rate', D.SIM_HZ / D.ANIM_FPS),
                        ('ground_rule', unreal.GSTN_MoveGroundRule.AIR if m['air'] else unreal.GSTN_MoveGroundRule.GROUND)):
        data.set_editor_property(prop, value)
    for window in m['windows']:
        first, last = D.tl(window[0]), D.tl(window[1])
        data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, first, last + 1, unreal.GSTN_PartSelector(), _boxes(key, window))
    if m['landing']:
        extra = []
        for first, last, what in m['landing']:
            start, end = D.tl(first), (D.tl(last) + 1 if last else m['total'])
            if what == 'auto':
                extra.append(f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Landing(StartFrame={start},EndFrame={end},Action=AutoCancel)')
            else:
                extra.append(f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Landing(StartFrame={start},EndFrame={end},Action=LandingLag,'
                             f'Lag=(RecoilFrames={what}))')
        _append_tracks(data, extra)
    text = data.export_text()
    counts = {k: text.count(f'GSTN_TimelineTrack_{k}(') for k in ('Hitbox', 'Landing')}
    name = f'TL_Overlord_Ref{D.ASSET[key]}'
    path = f'{DIR}/{name}'
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        asset = _tools().create_asset(name, DIR, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        log(f'created {path}')
    if not _guard.may_write(path):
        _guard.relink_montage(path, asset, montage)
        return asset
    asset.set_editor_property('data', data)
    log(f"{m['name']}: total {m['total']}, FAF {m['faf']}, tracks {counts}, ground rule {'Air' if m['air'] else 'Ground'}")
    return asset


def _ability(key, timeline):
    name = f'GA_Overlord_Ref{D.ASSET[key]}'
    path = f'{DIR}/{name}'
    bp = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not bp:
        factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GSTN_GA_TestKitStrike.static_class())
        asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
        bp = _tools().create_asset(name, DIR, asset_class, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'created {path}')
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    unreal.get_default_object(cls).set_editor_property('timeline_asset', timeline)
    return bp, cls


# ---- kit bindings
def _describe(b):
    req = b.get_editor_property('requirements')
    ability = b.get_editor_property('ability')
    return (f"{b.get_editor_property('command').get_editor_property('tag_name')} +{_names(req.get_editor_property('require_tags'))} "
            f"-{_names(req.get_editor_property('ignore_tags'))} -> {str(ability).rsplit('.', 1)[-1] if ability else None}")


def _context_of(b):
    """'air', 'ground' or None (a binding keyed on a link tag belongs to the link plugin and is never taken)"""
    req = b.get_editor_property('requirements')
    required = _names(req.get_editor_property('require_tags'))
    if any(t.startswith('Combat.Link') for t in required):
        return None
    return 'air' if AIRBORNE in required else 'ground'


def _bind(kit_name, classes):
    path = gc.path(kit_name)
    kit = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not kit:
        log(f'WARNING: {kit_name} not found, not bound')
        return None
    _backup(path)
    bindings = list(kit.get_editor_property('kit_bindings'))
    log(f'{kit_name} kit bindings before: ' + ('; '.join(_describe(b) for b in bindings) or 'none'))
    template = bindings[0] if bindings else None
    if template is None:
        weapon = kit.get_editor_property('primary_weapon')
        template = list(weapon.get_editor_property('bindings'))[0] if weapon and weapon.get_editor_property('bindings') else None
    if template is None:
        log(f'ERROR: {kit_name} has no binding to copy the struct from')
        return None
    ours = set(classes.values())
    front = []
    for key, cls in classes.items():
        m = D.MOVES[key]
        context = 'air' if m['air'] else 'ground'
        same = [i for i, b in enumerate(bindings) if str(b.get_editor_property('command').get_editor_property('tag_name')) == m['command']
                and _context_of(b) == context]
        if same:
            for i in same:
                b = bindings[i].copy()
                b.set_editor_property('ability', cls)
                b.set_editor_property('timeline_override', None)
                bindings[i] = b
            continue
        b = template.copy()
        req = b.get_editor_property('requirements')
        req.set_editor_property('require_tags', _container([AIRBORNE] if m['air'] else []))
        req.set_editor_property('ignore_tags', _container([LINKED] if m['air'] else [AIRBORNE, LINKED]))
        b.set_editor_property('requirements', req)
        b.set_editor_property('command', _tag(m['command']))
        b.set_editor_property('ability', cls)
        b.set_editor_property('timeline_override', None)
        front.append(b)
    # A rerun: the new bindings of the last run are found above by command and context and updated in place, so nothing stacks
    bindings = front + bindings
    kit.set_editor_property('kit_bindings', bindings)
    log(f'{kit_name} kit bindings after: ' + '; '.join(_describe(b) for b in bindings))
    weapon = kit.get_editor_property('primary_weapon')
    if weapon:
        for b in weapon.get_editor_property('bindings'):
            if str(b.get_editor_property('command').get_editor_property('tag_name')) in {D.MOVES[k]['command'] for k in classes} \
                    and b.get_editor_property('ability') not in ours:
                log(f'  weapon {weapon.get_name()} also binds {_describe(b)} (the kit binding comes first)')
    return kit


def _capsule_overrides(pairs):
    bp = unreal.load_asset(mcc.CAPSULE_ABP)
    if not bp:
        log(f'WARNING: {mcc.CAPSULE_ABP} missing: the capsule will not show the clips (run Tools/GAS/greybox/setup_greybox_assets.py)')
        return None
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    current = {k: v for k, v in cdo.get_editor_property('move_overrides').items()}
    current.update(pairs)
    cdo.set_editor_property('move_overrides', current)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f'capsule ABP: {len(pairs)} move overrides merged ({len(current)} in all)')
    return bp


def run(force=False):
    global _guard
    del _lines[:]
    ok = False
    try:
        _guard = setup_guard.Guard.for_unreal('setup_overlord_ref_moves', force=force, log=log)
        _guard.precheck([f'{DIR}/TL_Overlord_Ref{n}' for n in D.ASSET.values()])
        for folder in (DIR, CLIP_DIR):
            if not unreal.EditorAssetLibrary.does_directory_exist(folder):
                unreal.EditorAssetLibrary.make_directory(folder)
        rig = mcc.Rig(unreal.load_asset(mcc.MANNEQUIN).get_editor_property('skeleton'))
        saves, classes, overrides = [], {}, {}
        for key in D.MOVES:
            clip = _clip(rig, key)
            montage = _montage(key, clip)
            timeline = _timeline(key, montage)
            bp, classes[key] = _ability(key, timeline)
            overrides[montage] = clip
            saves += [clip, montage, timeline, bp]
        kits = [k for k in (_bind(name, classes) for name in KITS) if k]
        capsule = _capsule_overrides(overrides) if CAPSULE else None
        _guard.save_and_record(saves + kits + ([capsule] if capsule else []), unreal.EditorAssetLibrary.save_loaded_asset)
        log(f'saved {len(saves)} move assets, kits {[k.get_name() for k in kits]}'
            f"{', capsule ABP' if capsule else ''}; {_guard.summary()}; backups in {BACKUP}")
        ok = True
    except Exception as e:
        import traceback
        log(f'ERROR: {e!r}')
        log(traceback.format_exc())
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if any(w in l for w in ('WARNING', 'ERROR', 'KEPT'))][:4] + [_lines[-1][:300] if _lines else 'no output']:
        print(line)
    print('ESITO: ' + ('OK' if ok else 'FAIL') + ' (Saved/OverlordRefMoves/setup_overlord_ref_moves.txt)')
    return ok


if __name__ != 'setup_overlord_ref_moves':   # run as a script; imported, it waits for run()
    run()
