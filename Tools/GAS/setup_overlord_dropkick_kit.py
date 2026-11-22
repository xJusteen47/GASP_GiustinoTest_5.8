# ANIM-R2: the Overlord's dropkick (forward-air) on a test kit, so the built clips can be watched in a real Play session. Same idea and helpers as
# setup_overlord_dair_kit.py. Data only: no C++, no input mapping, no project setting, no Blueprint graph, no shared Blueprint edited by hand.
# Idempotent on its own assets; never overwrites hand edits: AS/AM/TL edited by hand (or saved by anything but this script) are KEPT, see
# Tools/GAS/setup_guard.py; run(force=True) or run(force=['TL_Overlord_Dropkick']) (env SETUP_FORCE) replaces them after a backup in
# Saved/Backups/SetupGuard. It only creates NEW assets (KIT_Test_Hits and WPN_Test_Hits are only read).
#
# Reads the three built clips of the anim pipeline (Content/AnimPipeline is a junction shared by every worktree) and copies them. Writes, under
# /Game/GASNPPTest/Combat/Overlord (new assets only):
#   AS_OverlordDropkick_Startup / _Loop / _Land   copies of /Game/AnimPipeline/Rig/Overlord/Dropkick/{Startup,Loop,Land}/AS_OverlordDropkick_*
#   AM_OverlordDropkick       montage of the three (startup 16 frames, loop x LOOP_REPS (4 by default, Tools/AnimPipeline/dropkick_loop_reps.py), land 32 frames), 2 frame blend in, 4 frame blend out
#                             (the loop plays 3 times: a hold until landing needs a looping section or the gameplay ability, game side)
#   TL_Overlord_Dropkick      UGSTN_CombatTimeline: hitbox active from startup f15 (timing CSV) to the end of the loop, FAF = last frame
#   GA_Overlord_Dropkick      thin Blueprint child of UGSTN_GA_TestKitStrike whose TimelineAsset is the timeline above (no graph)
#   WPN_Test_OverlordDropkick copy of WPN_Test_Hits plus ONE binding: Command.Attack.Directional (attack + stick forward) while Input.Context.Airborne
#   KIT_Test_OverlordDropkick copy of KIT_Test_Hits with that weapon. In game: L_Test_Hub, pick this kit, jump, hold stick forward, press attack
# The hit numbers and the hitbox shape are PROVISIONAL (owner must confirm). OVERLORD_DROPKICK_REFRESH=1 (or run(refresh=True)) re-copies the
# clips and rebuilds the montage after a new `animpipe build`.
#
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_overlord_dropkick_kit as s; s.run()
# Output:    Saved/AnimR2/setup_overlord_dropkick_kit.txt, at most 5 printed lines.
import csv
import os
import re
import runpy
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_guard  # noqa: E402

SRC = '/Game/AnimPipeline/Rig/Overlord/Dropkick'
CLIPS = [('Startup', f'{SRC}/Startup/AS_OverlordDropkick_Startup'), ('Loop', f'{SRC}/Loop/AS_OverlordDropkick_Loop'),
         ('Land', f'{SRC}/Land/AS_OverlordDropkick_Land')]
LOOP_REPS = runpy.run_path(os.path.join(unreal.Paths.project_dir(), 'Tools/AnimPipeline/dropkick_loop_reps.py'))['LOOP_REPS']   # shared with assemble_dropkick_montage.py
ROOT = '/Game/GASNPPTest/Combat'
DIR = ROOT + '/Overlord'
SOURCE_KIT, SOURCE_WEAPON = 'KIT_Test_Hits', 'WPN_Test_Hits'
COMMAND = 'Command.Attack.Directional'        # attack + stick forward
AIRBORNE_TAG = 'Input.Context.Airborne'
BLEND_IN_SECONDS = 2.0 / 60.0
BLEND_OUT_SECONDS = 4.0 / 60.0
SIM_HZ, ANIM_FPS = 62.5, 60.0
MONTAGE_PLAY_RATE = SIM_HZ / ANIM_FPS          # one animation frame per simulation tick: timeline frame N shows animation frame N
TIMING_CSV = os.path.join(unreal.Paths.project_dir(), 'Tools/AnimPipeline/cards/overlord/dropkick/timing/OverlordDropkick_timing.csv')
FALLBACK_ACTIVE_START = 15                     # startup f15 (the CSV rows are 1-based: row 16 is the first active one)
FOOT_BONE = 'foot_l'
# PROVISIONAL (owner must confirm), same scale as the down-air kit
HIT_RADIUS = 40.0
HIT_DAMAGE = 10.0
HIT_BASE_KNOCKBACK = 400.0
HIT_ANGLE_DEG = 40.0
HIT_VICTIM_HITSTOP = 6
HIT_ATTACKER_HITSTOP = 4
CAPSULE_HALF_HEIGHT = 90.0                     # cm, the mesh component sits this far below the root

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimR2', 'setup_overlord_dropkick_kit.txt')
_lines = []
_guard = None   # setup_guard.Guard of the current run


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_dropkick_kit] {msg}')


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


def _active_start():
    try:
        with open(TIMING_CSV) as f:
            rows = [r for r in csv.DictReader(f) if r['active'].strip().lower() == 'true']
        return int(rows[0]['frame']) - 1
    except Exception as e:
        log(f'WARNING: timing csv not read ({e!r}): active start {FALLBACK_ACTIVE_START}')
        return FALLBACK_ACTIVE_START


def _clips(refresh):
    lib = unreal.EditorAssetLibrary
    out = {}
    for name, src in CLIPS:
        path = f'{DIR}/AS_OverlordDropkick_{name}'
        if refresh and lib.does_asset_exist(path) and _guard.may_write(path):
            lib.delete_asset(path)
        if not lib.does_asset_exist(path):
            if not lib.does_asset_exist(src) or not lib.duplicate_asset(src, path):
                raise RuntimeError(f'could not copy {src} to {path} (run animpipe build first)')
            log(f'created {path}')
        out[name] = unreal.load_asset(path)
    return out


def _segment(anim, start, reps):
    seg = unreal.AnimSegment()
    for prop, value in (('anim_reference', anim), ('anim_start_time', 0.0), ('anim_end_time', anim.get_play_length()),
                        ('anim_play_rate', 1.0), ('looping_count', reps)):
        seg.set_editor_property(prop, value)
    # StartPos is read-only from Python in UE 5.9: import_text on a copy of the struct accepts it
    out = seg.copy()
    out.import_text(re.sub(r'StartPos=[-0-9.e]+', 'StartPos=%f' % start, seg.export_text()))
    return out


def _montage(clips, refresh):
    lib = unreal.EditorAssetLibrary
    path = f'{DIR}/AM_OverlordDropkick'
    if refresh and lib.does_asset_exist(path) and _guard.may_write(path):
        lib.delete_asset(path)
    montage = unreal.load_asset(path) if lib.does_asset_exist(path) else None
    lens = {k: v.get_play_length() for k, v in clips.items()}
    if montage and not _guard.may_write(path):
        return montage, lens
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', clips['Startup'])
        factory.set_editor_property('target_skeleton', clips['Startup'].get_editor_property('skeleton'))
        montage = _tools().create_asset('AM_OverlordDropkick', DIR, unreal.AnimMontage, factory)
        log(f'created {path}')
    segs = [_segment(clips['Startup'], 0.0, 1), _segment(clips['Loop'], lens['Startup'], LOOP_REPS),
            _segment(clips['Land'], lens['Startup'] + LOOP_REPS * lens['Loop'], 1)]
    track = unreal.AnimTrack()
    track.set_editor_property('anim_segments', segs)
    slot = unreal.SlotAnimationTrack()
    slot.set_editor_property('slot_name', 'DefaultSlot')
    slot.set_editor_property('anim_track', track)
    montage.set_editor_property('slot_anim_tracks', [slot])
    for prop, seconds in (('blend_in', BLEND_IN_SECONDS), ('blend_out', BLEND_OUT_SECONDS)):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', seconds)
        montage.set_editor_property(prop, blend)
    lib.save_loaded_asset(montage, False)
    _guard.record([path])
    # the montage length is only recomputed on load: reload the saved package
    unreal.EditorLoadingAndSavingUtils.reload_packages([montage.get_outermost()], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    montage = unreal.load_asset(path)
    log(f'montage {montage.get_play_length() * ANIM_FPS:.1f} frames (startup {lens["Startup"] * ANIM_FPS:.0f}, loop {lens["Loop"] * ANIM_FPS:.0f} '
        f'x{LOOP_REPS}, land {lens["Land"] * ANIM_FPS:.0f})')
    return montage, lens


def _foot(anim, frame):
    """Root-space offset of the foot at `frame` of `anim` (component space looks along +Y, the root along +X: (x, y) -> (y, -x))"""
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, frame / ANIM_FPS, unreal.AnimPoseEvaluationOptions())
    p = unreal.AnimPoseExtensions.get_bone_pose(pose, FOOT_BONE, unreal.AnimPoseSpaces.WORLD).translation
    return (p.y, -p.x, p.z - CAPSULE_HALF_HEIGHT)


def _timeline(montage, clips, lens):
    start = _active_start()
    loop_end = int(round((lens['Startup'] + LOOP_REPS * lens['Loop']) * ANIM_FPS))     # first land frame
    total = int(round(montage.get_play_length() * ANIM_FPS)) + 1
    faf = total - 1
    data = unreal.GSTN_CombatTimelineData()
    for prop, value in (('total_frames', total), ('first_actionable_frame', faf), ('override_frames', 0), ('montage', montage),
                        ('montage_play_rate', MONTAGE_PLAY_RATE)):
        data.set_editor_property(prop, value)
    hit = unreal.GSTN_HitData()
    for prop, value in (('damage', HIT_DAMAGE), ('base_knockback', HIT_BASE_KNOCKBACK), ('angle_deg', HIT_ANGLE_DEG),
                        ('hitstop_mode', unreal.GSTN_HitstopMode.OVERRIDE), ('hitstop_frames_override', HIT_VICTIM_HITSTOP),
                        ('attacker_hitstop_frames_override', HIT_ATTACKER_HITSTOP)):
        hit.set_editor_property(prop, value)
    shape = unreal.GSTN_HitShape()
    shape.set_editor_property('type', unreal.GSTN_ShapeType.SPHERE)
    shape.set_editor_property('extent', unreal.Vector(HIT_RADIUS, HIT_RADIUS, HIT_RADIUS))
    # hitbox keys count from the track start: the strike pose (startup f15), a loop frame, the strike pose again at the end of the loop
    # (loop f11 == startup f15)
    samples = [(0, _foot(clips['Startup'], start)), (5, _foot(clips['Loop'], 5)), (loop_end - start - 1, _foot(clips['Startup'], start))]
    keys = []
    for frame, (x, y, z) in samples:
        key = unreal.GSTN_HitboxKey()
        key.set_editor_property('frame', frame)
        key.set_editor_property('offset', unreal.Vector(x, y, z))
        keys.append(key)
    box = unreal.GSTN_HitboxSpec()
    for prop, value in (('shape', shape), ('space', unreal.GSTN_HitboxSpace.ROOT), ('hit_group', 0), ('data', hit), ('keys', keys),
                        ('sweep_between_frames', True)):
        box.set_editor_property(prop, value)
    data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, start, loop_end, unreal.GSTN_PartSelector(), [box])
    name = 'TL_Overlord_Dropkick'
    asset = unreal.load_asset(f'{DIR}/{name}') if unreal.EditorAssetLibrary.does_asset_exist(f'{DIR}/{name}') else None
    if not asset:
        asset = _tools().create_asset(name, DIR, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        log(f'created {DIR}/{name}')
    if not _guard.may_write(f'{DIR}/{name}'):
        _guard.relink_montage(f'{DIR}/{name}', asset, montage)
        return asset
    asset.set_editor_property('data', data)
    log(f'timeline total {total}, FAF {faf}, hitbox active {start}-{loop_end - 1} on {FOOT_BONE}, '
        f'keys {[tuple(round(v, 1) for v in k[1]) for k in samples]}')
    return asset


def _ability(timeline):
    name = 'GA_Overlord_Dropkick'
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


def _describe(bindings):
    out = []
    for b in bindings:
        req = b.get_editor_property('requirements')
        ability = b.get_editor_property('ability')
        out.append(f"{b.get_editor_property('command').get_editor_property('tag_name')} +{_names(req.get_editor_property('require_tags'))} "
                   f"-{_names(req.get_editor_property('ignore_tags'))} -> {str(ability).rsplit('.', 1)[-1] if ability else None}")
    return out


def _copy(source, name):
    path = f'{ROOT}/{name}'
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(f'{ROOT}/{source}', path):
            log(f'ERROR: could not copy {source} to {name}')
            return None
        log(f'created {path} (copy of {source})')
    return unreal.load_asset(path)


def _air_binding(template, cls):
    """A copy of `template` (any binding of the source weapon: it carries the right struct types) for the dropkick: command Directional (attack +
    stick forward), the Airborne tag required and nothing ignored"""
    out = template.copy()
    req = out.get_editor_property('requirements')
    req.set_editor_property('require_tags', _container([AIRBORNE_TAG]))
    req.set_editor_property('ignore_tags', _container([]))
    out.set_editor_property('requirements', req)
    out.set_editor_property('command', _tag(COMMAND))
    out.set_editor_property('ability', cls)
    return out


def _weapon_and_kit(cls):
    weapon, kit = _copy(SOURCE_WEAPON, 'WPN_Test_OverlordDropkick'), _copy(SOURCE_KIT, 'KIT_Test_OverlordDropkick')
    if not weapon or not kit:
        return []
    weapon.set_editor_property('display_name', 'WPN_Test_OverlordDropkick')
    weapon.set_editor_property('weapon_tags', _container(['Weapon.Test.OverlordDropkick', 'Test.Role.Primary']))
    old = list(weapon.get_editor_property('bindings'))
    log('weapon bindings before: ' + '; '.join(_describe(old)))
    if not old:
        log(f'ERROR: {SOURCE_WEAPON} has no binding to copy the struct from')
        return []
    # A rerun finds its own binding (same ability class) and replaces it
    new = [b for b in old if b.get_editor_property('ability') != cls] + [_air_binding(old[0], cls)]
    weapon.set_editor_property('bindings', new)
    log('weapon bindings after: ' + '; '.join(_describe(new)))
    kit.set_editor_property('display_name', 'OVERLORD DROPKICK')
    kit.set_editor_property('kit_tags', _container(['Kit.Test.OverlordDropkick']))
    kit.set_editor_property('primary_weapon', weapon)
    kit_bindings = list(kit.get_editor_property('kit_bindings'))
    log('kit bindings: ' + '; '.join(_describe(kit_bindings)))
    if any(str(b.get_editor_property('command').get_editor_property('tag_name')) == COMMAND for b in kit_bindings):
        log(f'WARNING: the kit itself binds {COMMAND}: check which binding wins')
    return [weapon, kit]


def run(refresh=False, force=False):
    """force: True, or asset names (['TL_Overlord_Dropkick']) to overwrite even if edited by hand (backed up first); default keeps them"""
    global _guard
    del _lines[:]
    ok = False
    try:
        _guard = setup_guard.Guard.for_unreal('setup_overlord_dropkick_kit', force=force, log=log)
        _guard.precheck([f'{DIR}/AS_OverlordDropkick_{name}' for name, _ in CLIPS] + [f'{DIR}/AM_OverlordDropkick', f'{DIR}/TL_Overlord_Dropkick'])
        if not unreal.EditorAssetLibrary.does_directory_exist(DIR):
            unreal.EditorAssetLibrary.make_directory(DIR)
        clips = _clips(refresh)
        montage, lens = _montage(clips, refresh)
        timeline = _timeline(montage, clips, lens)
        bp, cls = _ability(timeline)
        tail = _weapon_and_kit(cls)
        if not tail:
            raise RuntimeError('weapon or kit not built')
        saves = list(clips.values()) + [timeline, bp] + tail
        _guard.save_and_record(saves, unreal.EditorAssetLibrary.save_loaded_asset)
        log('saved: ' + ', '.join(a.get_name() for a in saves + [montage]) + '; ' + _guard.summary())
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if any(w in l for w in ('WARNING', 'ERROR', 'KEPT', 'FORCE', 'MERGE'))][:4] + [_lines[-1][:300]]:
        print(line)
    return ok


if __name__ == '__main__':
    run(refresh=os.environ.get('OVERLORD_DROPKICK_REFRESH') == '1')
