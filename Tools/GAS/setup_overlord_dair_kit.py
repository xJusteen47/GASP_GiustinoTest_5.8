# ANIM-DAIR-1: the Overlord's down-air on a test kit, so the built animation can be watched in a real Play session (same idea and same helpers
# as setup_overlord_jab_kit.py). Data only: no C++, no input mapping, no project setting, no Blueprint graph. Idempotent: a rerun rewrites the specs
# below over hand edits. Like the jab script it only ever creates NEW assets (nothing existing is overwritten except the assets it made itself),
# so there is nothing to back up: KIT_Test_Hits, WPN_Test_Hits, the jab kit and the animation builds are only read.
#
# Reads (never writes) the built animation(s) of the anim pipeline. Which one: the environment variable OVERLORD_DAIR_ANIM, a comma separated
# list of asset paths (default ANIM_V8). A list builds one kit per animation in the same run, so both can be in the same map session:
#   OVERLORD_DAIR_ANIM=/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV8_Build,/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV7_Build
# The label (V8, V7) is the 'V<n>' of the asset name and ends every asset name, so the kits never overwrite each other and a later run with the
# other value adds its kit next to the first one. Content/AnimPipeline is shared by every worktree through a junction: this script only duplicates
# out of it.
# Writes, under /Game/GASNPPTest/Combat/Overlord (new assets only), for each label L:
#   AS_OverlordDAir<L>        copy of the built animation (rerun with refresh=True after a new `animpipe build`)
#   AM_OverlordDAir<L>        its montage (default slot, 2 frame blend in, play rate MONTAGE_PLAY_RATE)
#   TL_Overlord_DAir<L>       UGSTN_CombatTimeline: startup 0-14, hitbox active 15-17, FAF 42, 43 frames; the hitbox follows the left foot
#   GA_Overlord_DAir<L>       thin Blueprint child of UGSTN_GA_TestKitStrike whose TimelineAsset is the timeline above (no graph)
#   WPN_Test_OverlordDAir<L>  copy of WPN_Test_Hits plus ONE binding: Command.Attack.CrouchNeutral (down + attack) while Input.Context.Airborne
#   KIT_Test_OverlordDAir<L>  copy of KIT_Test_Hits with that weapon. In game: L_Test_Hub, pick this kit, jump, hold crouch and press attack
# The data model expresses "airborne only" as a binding requirement (combat.md 12, tag Input.Context.Airborne), so nothing else is needed: on the
# ground the binding does not match and the kit's own grounded CrouchNeutral binding (if any) wins.
# Frame data is from the move card (0-based animation frames at 60 fps) and is NOT to be changed here. The hit numbers and the hitbox shape are
# PROVISIONAL (owner must confirm), see below.
#
# Headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_overlord_dair_kit.py" -unattended -nullrhi
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_overlord_dair_kit as s; s.run()
# Output:    Saved/AnimDAir/setup_overlord_dair_kit.txt, at most 5 printed lines.
import math
import os
import re

import unreal

ANIM_V8 = '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV8_Build'
ANIM_V7 = '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV7_Build'   # the base, for the comparison capture
ROOT = '/Game/GASNPPTest/Combat'
DIR = ROOT + '/Overlord'
SOURCE_KIT, SOURCE_WEAPON = 'KIT_Test_Hits', 'WPN_Test_Hits'
COMMAND = 'Command.Attack.CrouchNeutral'      # crouch (down) held + attack
AIRBORNE_TAG = 'Input.Context.Airborne'
BLEND_IN_SECONDS = 2.0 / 60.0
# Same reason as the jab kit: the montage advances one animation frame per simulation tick, so timeline frame N shows animation frame N
SIM_HZ, ANIM_FPS = 62.5, 60.0
MONTAGE_PLAY_RATE = SIM_HZ / ANIM_FPS
# Frame data from the card (0-based animation frames at 60 fps): NOT to be changed
TOTAL_FRAMES, FAF, ACTIVE = 43, 42, (15, 17)
FOOT_BONE = 'foot_l'

# PROVISIONAL (owner must confirm): nothing below was approved, scale chosen to sit above the jab kit (jab 2: damage 7, knockback 550)
HIT_RADIUS = 40.0              # cm, sphere on the foot
HIT_DAMAGE = 11.0
HIT_BASE_KNOCKBACK = 450.0
HIT_ANGLE_DEG = 75.0           # steep upward pop, not a meteor spike: a spike into the floor needs the reaction rules checked first
HIT_VICTIM_HITSTOP = 7         # frames
HIT_ATTACKER_HITSTOP = 5
# PROVISIONAL: where the foot is in the root space when the bone cannot be sampled from the animation (root = capsule centre, x forward). The
# bone is sampled by default; this is only the fallback, and the offset of the mesh below the capsule centre used to turn component space into root space
FALLBACK_FOOT_KEYS = ((28.0, 0.0, -95.0), (28.0, 0.0, -105.0))
CAPSULE_HALF_HEIGHT = 90.0     # cm, the mesh component sits this far below the root

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimDAir', 'setup_overlord_dair_kit.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_dair_kit] {msg}')


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


def _label(path):
    m = re.search(r'V(\d+)', path.rsplit('/', 1)[-1])
    return f'V{m.group(1)}' if m else 'X'


def _anim_and_montage(label, src, refresh):
    anim_path, montage_path = f'{DIR}/AS_OverlordDAir{label}', f'{DIR}/AM_OverlordDAir{label}'
    lib = unreal.EditorAssetLibrary
    if refresh:
        for p in (montage_path, anim_path):
            if lib.does_asset_exist(p):
                lib.delete_asset(p)
                log(f'refresh: deleted {p}')
    if not lib.does_asset_exist(anim_path):
        if not lib.does_asset_exist(src) or not lib.duplicate_asset(src, anim_path):
            log(f'ERROR: could not copy {src} to {anim_path}')
            return None, None
        log(f'created {anim_path} (copy of {src})')
    anim = unreal.load_asset(anim_path)
    montage = unreal.load_asset(montage_path) if lib.does_asset_exist(montage_path) else None
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', anim)
        factory.set_editor_property('target_skeleton', anim.get_editor_property('skeleton'))
        montage = _tools().create_asset(f'AM_OverlordDAir{label}', DIR, unreal.AnimMontage, factory)
        log(f'created {montage_path}')
    blend = montage.get_editor_property('blend_in')
    blend.set_editor_property('blend_time', BLEND_IN_SECONDS)
    montage.set_editor_property('blend_in', blend)
    frames = anim.get_play_length() * ANIM_FPS
    log(f'{label}: anim {frames:.1f} frames at 60 fps (card: {TOTAL_FRAMES}), skeleton {anim.get_editor_property("skeleton").get_name()}, '
        f'montage {montage.get_play_length():.3f} s')
    return anim, montage


def _foot_keys(anim):
    """Root-space offsets of the foot at the first and last active frame, sampled from the animation (the hitbox keys count from the track start).
    Returns (keys, how). Falls back to FALLBACK_FOOT_KEYS when the pose cannot be evaluated from Python (the report says which one was used)"""
    out = []
    try:
        opts = unreal.AnimPoseEvaluationOptions()
        pass
        for frame in (ACTIVE[0], ACTIVE[1]):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, frame / ANIM_FPS, opts)
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, FOOT_BONE, unreal.AnimPoseSpaces.WORLD)
            p = t.translation
            # Component space of the mannequin looks along +Y while the root looks along +X (the mesh is turned -90 degrees): (x, y) -> (y, -x)
            out.append((p.y, -p.x, p.z - CAPSULE_HALF_HEIGHT))
        return out, 'sampled from ' + FOOT_BONE
    except Exception as e:
        log(f'WARNING: could not sample {FOOT_BONE} ({e!r}): using the provisional fallback keys')
        return list(FALLBACK_FOOT_KEYS), 'FALLBACK constants'


def _timeline(label, anim, montage):
    data = unreal.GSTN_CombatTimelineData()
    for prop, value in (('total_frames', TOTAL_FRAMES), ('first_actionable_frame', FAF), ('override_frames', 0), ('montage', montage),
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
    foot, how = _foot_keys(anim)
    keys = []
    for frame, (x, y, z) in zip((0, ACTIVE[1] - ACTIVE[0]), foot):
        key = unreal.GSTN_HitboxKey()
        key.set_editor_property('frame', frame)
        key.set_editor_property('offset', unreal.Vector(x, y, z))
        keys.append(key)
    box = unreal.GSTN_HitboxSpec()
    for prop, value in (('shape', shape), ('space', unreal.GSTN_HitboxSpace.ROOT), ('hit_group', 0), ('data', hit), ('keys', keys),
                        ('sweep_between_frames', True)):
        box.set_editor_property(prop, value)
    data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, ACTIVE[0], ACTIVE[1] + 1, unreal.GSTN_PartSelector(), [box])
    name = f'TL_Overlord_DAir{label}'
    asset = unreal.load_asset(f'{DIR}/{name}') if unreal.EditorAssetLibrary.does_asset_exist(f'{DIR}/{name}') else None
    if not asset:
        asset = _tools().create_asset(name, DIR, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        log(f'created {DIR}/{name}')
    asset.set_editor_property('data', data)
    log(f'{label}: timeline total {TOTAL_FRAMES}, FAF {FAF}, active {ACTIVE}, hitbox on {FOOT_BONE} keys {[tuple(round(v, 1) for v in k) for k in foot]} ({how})')
    return asset


def _ability(label, timeline):
    name = f'GA_Overlord_DAir{label}'
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
    """A copy of `template` (any binding of the source weapon: it carries the right struct types) for the down-air: command CrouchNeutral, the
    Airborne tag required and nothing ignored"""
    out = template.copy()
    req = out.get_editor_property('requirements')
    req.set_editor_property('require_tags', _container([AIRBORNE_TAG]))
    req.set_editor_property('ignore_tags', _container([]))
    out.set_editor_property('requirements', req)
    out.set_editor_property('command', _tag(COMMAND))
    out.set_editor_property('ability', cls)
    return out


def _weapon_and_kit(label, cls):
    weapon, kit = _copy(SOURCE_WEAPON, f'WPN_Test_OverlordDAir{label}'), _copy(SOURCE_KIT, f'KIT_Test_OverlordDAir{label}')
    if not weapon or not kit:
        return []
    weapon.set_editor_property('display_name', f'WPN_Test_OverlordDAir{label}')
    weapon.set_editor_property('weapon_tags', _container([f'Weapon.Test.OverlordDAir', 'Test.Role.Primary']))
    old = list(weapon.get_editor_property('bindings'))
    log('weapon bindings before: ' + '; '.join(_describe(old)))
    if not old:
        log(f'ERROR: {SOURCE_WEAPON} has no binding to copy the struct from')
        return []
    # A rerun finds its own binding (same ability class) and replaces it
    new = [b for b in old if b.get_editor_property('ability') != cls] + [_air_binding(old[0], cls)]
    weapon.set_editor_property('bindings', new)
    log('weapon bindings after: ' + '; '.join(_describe(new)))
    kit.set_editor_property('display_name', f'OVERLORD DAIR {label}')
    kit.set_editor_property('kit_tags', _container(['Kit.Test.OverlordDAir']))
    kit.set_editor_property('primary_weapon', weapon)
    kit_bindings = list(kit.get_editor_property('kit_bindings'))
    log('kit bindings: ' + '; '.join(_describe(kit_bindings)))
    if any(str(b.get_editor_property('command').get_editor_property('tag_name')) == COMMAND for b in kit_bindings):
        log(f'WARNING: the kit itself binds {COMMAND}: check which binding wins')
    return [weapon, kit]


def run(refresh=False, anims=None):
    del _lines[:]
    ok = False
    try:
        anims = anims or [a.strip() for a in os.environ.get('OVERLORD_DAIR_ANIM', ANIM_V8).split(',') if a.strip()]
        if not unreal.EditorAssetLibrary.does_directory_exist(DIR):
            unreal.EditorAssetLibrary.make_directory(DIR)
        saves = []
        for src in anims:
            label = _label(src)
            anim, montage = _anim_and_montage(label, src, refresh)
            timeline = _timeline(label, anim, montage) if montage else None
            if not timeline:
                raise RuntimeError(f'{label} not built')
            bp, cls = _ability(label, timeline)
            tail = _weapon_and_kit(label, cls)
            if not tail:
                raise RuntimeError(f'{label}: weapon or kit not built')
            saves += [anim, montage, timeline, bp] + tail
        for asset in saves:
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        log(f'saved {len(saves)} assets: ' + ', '.join(a.get_name() for a in saves))
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'WARNING' in l or 'ERROR' in l][:4] + [_lines[-1][:300]]:
        print(line)
    return ok


if __name__ == '__main__':
    run()
